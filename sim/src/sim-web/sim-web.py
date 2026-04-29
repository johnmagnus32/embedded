#!/usr/bin/env python3
"""Web debugger for the ARM Cortex-M4 emulator.

Spawns sim-core, connects via TCP, serves HTML UI.
Browser sends commands via HTTP, sim-web forwards to sim-core over TCP.

GET /       → HTML UI
GET /uart   → UART state from file
POST /cmd   → forward command to sim-core, return response
POST /log   → browser logs printed to terminal
"""

import argparse, json, os, socket, select, subprocess, sys, threading, time, struct, hashlib, base64

def log_web(msg):
    sys.stderr.write(f'[sim-web] {msg}\n')
    sys.stderr.flush()

_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_dir, "index.html")) as _f:
    HTML = _f.read()

SIM_PORT = 9001
UART_PORT = 9002
TRACE_PORT = 9003
DISPLAY_PORT = 9004

class WebDebugger:
    def __init__(self, machine, elf, port, extra_args):
        self.port = port
        self.machine = machine
        self.elf = elf
        self.extra_args = extra_args
        self.sim = None
        self.sim_sock = None
        self.last_state = '{}'

    def start_sim(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sim_core = os.path.join(script_dir, '..', '..', 'build', 'sim-core')
        cmd = [sim_core,
               '--machine', self.machine,
               '--firmware', self.elf,
               '--debug', str(SIM_PORT),
               '--chardev', f'usart2={UART_PORT}',
               '--chardev', f'trace={TRACE_PORT}',
               '--chardev', f'display={DISPLAY_PORT}'] + self.extra_args
        self.sim = subprocess.Popen(cmd, stderr=sys.stderr)
        self._buf = b''
        self.uart_buf = ''

        # Wait for sim-core to start listening
        for _ in range(50):
            try:
                self.sim_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sim_sock.connect(('127.0.0.1', SIM_PORT))
                self.sim_sock.settimeout(60)
                log_web(f'Connected to sim-core on port {SIM_PORT}')
                self.last_state = self._recv_line()
                break
            except ConnectionRefusedError:
                time.sleep(0.1)

        # Connect to UART stream
        try:
            self.uart_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.uart_sock.connect(('127.0.0.1', UART_PORT))
            log_web(f'Connected to UART on port {UART_PORT}')
            # Read UART in background thread
            def uart_reader():
                while True:
                    try:
                        data = self.uart_sock.recv(4096)
                        if not data: break
                        self.uart_buf += data.decode(errors='replace')
                    except: break
            threading.Thread(target=uart_reader, daemon=True).start()
        except:
            log_web('UART connection failed (will retry later)')

        # Connect to trace stream
        self.trace_events = []
        self.heap_blocks = {}  # addr -> {name, size, cy}
        try:
            self.trace_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.trace_sock.connect(('127.0.0.1', TRACE_PORT))
            log_web(f'Connected to trace port {TRACE_PORT}')
            def trace_reader():
                buf = ''
                while True:
                    try:
                        data = self.trace_sock.recv(4096)
                        if not data: break
                        buf += data.decode(errors='replace')
                        while '\n' in buf:
                            line, buf = buf.split('\n', 1)
                            line = line.strip()
                            # Format: "B:name@cycle" or "E@cycle" or "I:name@cycle"
                            cy = None
                            if '@' in line:
                                line, cystr = line.rsplit('@', 1)
                                try: cy = int(cystr)
                                except: pass
                            if line.startswith('B:') and cy is not None:
                                self.trace_events.append({'ctx': line[2:], 'type': 'B', 'cy': cy})
                            elif line.startswith('E') and cy is not None:
                                self.trace_events.append({'type': 'E', 'cy': cy})
                            elif line.startswith('I:') and cy is not None:
                                self.trace_events.append({'ctx': line[2:], 'type': 'I', 'cy': cy})
                            elif line.startswith('H:'):
                                # H:name@addr,size (hex) — heap alloc
                                try:
                                    name, rest = line[2:].split('@', 1)
                                    addr_s, size_s = rest.split(',', 1)
                                    addr = int(addr_s, 16)
                                    size = int(size_s, 16)
                                    self.heap_blocks[addr] = {'name': name, 'size': size, 'cy': cy or 0}
                                except: pass
                            elif line.startswith('F:'):
                                # F:addr — heap free
                                try:
                                    addr = int(line[2:], 16)
                                    self.heap_blocks.pop(addr, None)
                                except: pass
                    except: break
            threading.Thread(target=trace_reader, daemon=True).start()
        except:
            log_web('Trace not available')

        # Connect to display framebuffer stream
        self.display_sock = None
        self.display_frame = b''
        self.display_w = 240
        self.display_h = 320
        try:
            self.display_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.display_sock.connect(('127.0.0.1', DISPLAY_PORT))
            log_web(f'Connected to display port {DISPLAY_PORT}')
            FRAME_SIZE = 240 * 320 * 2
            HEADER_SIZE = 4
            def display_reader():
                buf = b''
                while True:
                    try:
                        data = self.display_sock.recv(262144)
                        if not data: break
                        buf += data
                        while len(buf) >= HEADER_SIZE:
                            ew = buf[0] | (buf[1] << 8)
                            eh = buf[2] | (buf[3] << 8)
                            fsz = ew * eh * 2
                            if len(buf) < HEADER_SIZE + fsz: break
                            self.display_w = ew
                            self.display_h = eh
                            self.display_frame = buf[HEADER_SIZE:HEADER_SIZE + fsz]
                            buf = buf[HEADER_SIZE + fsz:]
                    except: break
            threading.Thread(target=display_reader, daemon=True).start()

            # WebSocket display push thread
            self._ws_clients = []
            self._ws_lock = threading.Lock()
            self._prev_frame = b'\x00' * (240 * 320 * 2)  # init to black
            def ws_push_loop():
                push_count = [0]
                while True:
                    time.sleep(0.03)
                    raw = self.display_frame
                    if not raw: continue
                    prev = self._prev_frame
                    if prev is raw: continue
                    self._prev_frame = raw
                    # Build delta
                    if prev and len(prev) == len(raw):
                        delta = bytearray()
                        i = 0; n = len(raw)
                        while i < n:
                            if raw[i] != prev[i]:
                                start = i
                                while i < n and i - start < 65535 and raw[i] != prev[i]: i += 1
                                delta += struct.pack('<IH', start, i - start)
                                delta += raw[start:i]
                            else: i += 1
                        payload = bytes([1]) + bytes(delta)  # 1 = delta
                    else:
                        payload = bytes([0]) + raw  # 0 = full frame
                    # Push to all WS clients
                    frame = self._ws_encode(payload)
                    with self._ws_lock:
                        dead = []
                        for c in self._ws_clients:
                            try:
                                c.settimeout(0.5)
                                c.sendall(frame)
                            except:
                                dead.append(c)
                        for c in dead:
                            try: c.close()
                            except: pass
                            self._ws_clients.remove(c)
                        push_count[0] += 1
            threading.Thread(target=ws_push_loop, daemon=True).start()
        except:
            log_web('Display not available')

    def _ws_encode(self, data):
        """Encode a WebSocket binary frame."""
        b = bytearray([0x82])  # FIN + binary opcode
        n = len(data)
        if n < 126: b.append(n)
        elif n < 65536: b.append(126); b += struct.pack('>H', n)
        else: b.append(127); b += struct.pack('>Q', n)
        return bytes(b) + data

    def _ws_upgrade(self, conn, headers):
        """Perform WebSocket handshake."""
        key = None
        for line in headers.split('\r\n'):
            if line.lower().startswith('sec-websocket-key:'):
                key = line.split(':', 1)[1].strip()
        if not key: return False
        accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-5AB5DC525C75').encode()).digest()).decode()
        resp = (f'HTTP/1.1 101 Switching Protocols\r\n'
                f'Upgrade: websocket\r\nConnection: Upgrade\r\n'
                f'Sec-WebSocket-Accept: {accept}\r\n\r\n')
        conn.sendall(resp.encode())
        log_web(f'WS handshake response: {repr(resp[:150])}')
        return True

    def _recv_line(self):
        """Read one newline-terminated JSON line from sim-core."""
        while b'\n' not in self._buf:
            chunk = self.sim_sock.recv(262144)
            if not chunk:
                return '{}'
            self._buf += chunk
        line, self._buf = self._buf.split(b'\n', 1)
        return line.decode()

    def send_command(self, cmd_json):
        """Send a command to sim-core and return the response."""
        self.sim_sock.sendall((cmd_json + '\n').encode())
        resp = self._recv_line()
        self.last_state = resp
        return resp

    def send_command_async(self, cmd_json):
        """Send a command that may block (continue/run). Read response on background thread."""
        self.sim_sock.sendall((cmd_json + '\n').encode())
        def reader():
            resp = self._recv_line()
            self.last_state = resp
            self._async_resp = resp
        self._async_resp = None
        threading.Thread(target=reader, daemon=True).start()

    def http_response(self, conn, status, content_type, body):
        if isinstance(body, str): body = body.encode()
        hdr = (f'HTTP/1.1 {status}\r\n'
               f'Content-Type: {content_type}\r\n'
               f'Content-Length: {len(body)}\r\n'
               f'Connection: close\r\n'
               f'Access-Control-Allow-Origin: *\r\n\r\n')
        conn.sendall(hdr.encode() + body)
        conn.close()

    def run(self):
        self.start_sim()

        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(('', self.port))
        srv.listen(5)
        srv.setblocking(False)
        log_web(f'Debugger: http://localhost:{self.port}')

        while True:
            try:
                readable, _, _ = select.select([srv], [], [], 0.1)
            except:
                break

            for r in readable:
                conn, addr = srv.accept()
                try:
                    data = conn.recv(8192).decode(errors='ignore')
                except:
                    conn.close()
                    continue
                lines = data.split('\r\n')
                req = lines[0] if lines else ''
                if '/ws' in req: log_web(f'REQ: {req}')

                if req.startswith('GET /init'):
                    self.http_response(conn, '200 OK', 'application/json', self.last_state)

                elif req.startswith('GET /trace'):
                    import json as _json
                    heap = [{'addr': a, **v} for a, v in self.heap_blocks.items()]
                    self.http_response(conn, '200 OK', 'application/json',
                        _json.dumps({"timeline": self.trace_events[-512:], "heap": heap}))

                elif req.startswith('GET /uart'):
                    import json as _json
                    self.http_response(conn, '200 OK', 'application/json',
                        _json.dumps({"uart": self.uart_buf}))

                elif req.startswith('GET /ws-test'):
                    log_web(f'WS-TEST raw request: {repr(data[:500])}')
                    if self._ws_upgrade(conn, data):
                        log_web('WS test client connected')
                        def test_push(c):
                            time.sleep(0.1)  # let handshake settle
                            for i in range(10):
                                try:
                                    c.settimeout(2)
                                    msg = f'hello {i}'.encode()
                                    # Text frame: opcode 0x81
                                    frame = bytearray([0x81, len(msg)]) + msg
                                    c.sendall(bytes(frame))
                                    log_web(f'WS test sent: hello {i}')
                                    time.sleep(1)
                                except Exception as e:
                                    log_web(f'WS test error: {e}')
                                    break
                            try: c.close()
                            except: pass
                        threading.Thread(target=test_push, args=(conn,), daemon=True).start()
                        continue

                elif req.startswith('GET /ws-display'):
                    if self._ws_upgrade(conn, data):
                        log_web('WebSocket display client connected')
                        with self._ws_lock:
                            self._ws_clients.append(conn)
                        continue  # don't close conn

                elif req.startswith('GET /display'):
                    raw = self.display_frame
                    if raw:
                        # Delta compression: only send changed bytes
                        prev = getattr(self, '_prev_frame', None)
                        if prev and len(prev) == len(raw):
                            # Build delta: list of (offset, length, data) runs
                            import struct
                            delta = bytearray()
                            i = 0
                            n = len(raw)
                            while i < n:
                                if raw[i] != prev[i]:
                                    start = i
                                    while i < n and i - start < 65535 and raw[i] != prev[i]:
                                        i += 1
                                    delta += struct.pack('<IH', start, i - start)
                                    delta += raw[start:i]
                                else:
                                    i += 1
                            self._prev_frame = raw
                            ew = getattr(self, 'display_w', 240)
                            eh = getattr(self, 'display_h', 320)
                            hdr = (f'HTTP/1.1 200 OK\r\n'
                                   f'Content-Type: application/octet-stream\r\n'
                                   f'X-Width: {ew}\r\nX-Height: {eh}\r\n'
                                   f'X-Delta: 1\r\n'
                                   f'Content-Length: {len(delta)}\r\n'
                                   f'Access-Control-Expose-Headers: X-Width, X-Height, X-Delta\r\n'
                                   f'\r\n').encode()
                            conn.sendall(hdr)
                            conn.sendall(bytes(delta))
                        else:
                            # Full frame (first frame or size change)
                            self._prev_frame = raw
                            if not hasattr(self, '_display_hdr') or len(raw) != getattr(self, '_display_len', 0):
                                ew = getattr(self, 'display_w', 240)
                                eh = getattr(self, 'display_h', 320)
                                self._display_hdr = (f'HTTP/1.1 200 OK\r\n'
                                       f'Content-Type: application/octet-stream\r\n'
                                       f'X-Width: {ew}\r\nX-Height: {eh}\r\n'
                                       f'Content-Length: {len(raw)}\r\n'
                                       f'Access-Control-Expose-Headers: X-Width, X-Height\r\n'
                                       f'\r\n').encode()
                                self._display_len = len(raw)
                            conn.sendall(self._display_hdr)
                            conn.sendall(raw)
                    else:
                        self.http_response(conn, '200 OK', 'application/json', '{"w":0,"h":0}')

                elif req.startswith('POST /gpio'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    try:
                        self.sim_sock.sendall((body.strip() + '\n').encode())
                    except: pass
                    self.http_response(conn, '200 OK', 'application/json', '{"ok":true}')

                elif req.startswith('GET /status'):
                    resp = getattr(self, '_async_resp', None)
                    if resp:
                        self._async_resp = None
                        self.http_response(conn, '200 OK', 'application/json', resp)
                    else:
                        self.http_response(conn, '200 OK', 'application/json', '{"running":true}')

                elif req.startswith('POST /cmd'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    cmd = body.strip()
                    log_web(f'CMD: {cmd}')
                    try:
                        import json as _json
                        parsed = _json.loads(cmd)
                        if parsed.get('cmd') == 'continue':
                            self.send_command_async(cmd)
                            self.http_response(conn, '200 OK', 'application/json', '{"running":true}')
                        else:
                            resp = self.send_command(cmd)
                            self.http_response(conn, '200 OK', 'application/json', resp)
                    except Exception as e:
                        log_web(f'Error: {e}')
                        self.http_response(conn, '500 Error', 'application/json', '{"error":"sim-core disconnected"}')

                elif req.startswith('POST /log'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    sys.stderr.write(f'[sim-ui] {body}\n')
                    sys.stderr.flush()
                    self.http_response(conn, '200 OK', 'text/plain', '')

                elif req.startswith('GET'):
                    html_bytes = HTML.encode()
                    resp = f'HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: {len(html_bytes)}\r\n\r\n'
                    conn.sendall(resp.encode() + html_bytes)
                    conn.close()

                else:
                    conn.close()

def main():
    p = argparse.ArgumentParser(description='ARM Cortex-M4 Emulator + Web Debugger')
    p.add_argument('--machine', default='gameboy', help='Machine name')
    p.add_argument('--firmware', required=True, help='Firmware ELF file')
    args, extra = p.parse_known_args()

    dbg = WebDebugger(args.machine, args.firmware, 3000, extra)
    try:
        dbg.run()
    except KeyboardInterrupt:
        log_web('Stopped.')
        if dbg.sim: dbg.sim.kill()

if __name__ == '__main__':
    main()
