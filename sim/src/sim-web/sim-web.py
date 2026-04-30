#!/usr/bin/env python3
"""Web debugger for the ARM Cortex-M4 emulator.

Spawns sim-core, connects via TCP, serves HTML UI.
Browser sends commands via HTTP, sim-web forwards to sim-core over TCP.

GET /       → HTML UI
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
AUDIO_PORT = 9005
IO_PORT = 9006

class WebDebugger:
    def __init__(self, machine, elf, port, extra_args):
        self.port = port
        self.machine = machine
        self.elf = elf
        self.extra_args = extra_args
        self.sim = None
        self.sim_sock = None
        self.last_state = '{}'
        self._ws_status_client = None
        self._ws_trace_clients = []

    def start_sim(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sim_core = os.path.join(script_dir, '..', '..', 'build', 'sim-core')
        cmd = [sim_core,
               '--machine', self.machine,
               '--firmware', self.elf,
               '--debug', str(SIM_PORT),
               '--chardev', f'usart2={UART_PORT}',
               '--chardev', f'trace={TRACE_PORT}',
               '--chardev', f'display={DISPLAY_PORT}',
               '--chardev', f'audio={AUDIO_PORT}',
               '--chardev', f'io={IO_PORT}'] + self.extra_args
        self.sim = subprocess.Popen(cmd, stderr=sys.stderr)
        self._buf = b''
        self.uart_buf = ''

        # Wait for sim-core to start listening
        for _ in range(50):
            try:
                self.sim_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sim_sock.connect(('127.0.0.1', SIM_PORT))
                self.sim_sock.settimeout(None)  # blocking — async reader waits indefinitely
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
            self._ws_uart_clients = []
            def uart_reader():
                while True:
                    try:
                        data = self.uart_sock.recv(4096)
                        if not data: break
                        self.uart_buf += data.decode(errors='replace')
                        # Push to WebSocket UART clients
                        frame = self._ws_encode(data)  # send raw bytes as binary
                        dead = []
                        for c in self._ws_uart_clients:
                            try: c.sendall(frame)
                            except: dead.append(c)
                        for c in dead: self._ws_uart_clients.remove(c)
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
                import json as _json
                buf = ''
                while True:
                    try:
                        data = self.trace_sock.recv(4096)
                        if not data: break
                        buf += data.decode(errors='replace')
                        while '\n' in buf:
                            line, buf = buf.split('\n', 1)
                            line = line.strip()
                            cy = None
                            if '@' in line:
                                line, cystr = line.rsplit('@', 1)
                                try: cy = int(cystr)
                                except: pass
                            evt = None
                            if line.startswith('B:') and cy is not None:
                                evt = {'ctx': line[2:], 'type': 'B', 'cy': cy}
                            elif line.startswith('E') and cy is not None:
                                evt = {'type': 'E', 'cy': cy}
                            elif line.startswith('I:') and cy is not None:
                                evt = {'ctx': line[2:], 'type': 'I', 'cy': cy}
                            elif line.startswith('H:'):
                                try:
                                    name, rest = line[2:].split('@', 1)
                                    addr_s, size_s = rest.split(',', 1)
                                    addr = int(addr_s, 16)
                                    size = int(size_s, 16)
                                    self.heap_blocks[addr] = {'name': name, 'size': size, 'cy': cy or 0}
                                    evt = {'type': 'H', 'name': name, 'addr': addr, 'size': size, 'cy': cy or 0}
                                except: pass
                            elif line.startswith('F:'):
                                try:
                                    addr = int(line[2:], 16)
                                    self.heap_blocks.pop(addr, None)
                                    evt = {'type': 'F', 'addr': addr}
                                except: pass
                            if evt:
                                if evt['type'] in ('B', 'E', 'I'):
                                    self.trace_events.append(evt)
                                # Push to WS clients
                                dead = []
                                for c in self._ws_trace_clients:
                                    try: c.sendall(self._ws_encode(_json.dumps(evt).encode()))
                                    except: dead.append(c)
                                for c in dead: self._ws_trace_clients.remove(c)
                    except: break
            threading.Thread(target=trace_reader, daemon=True).start()
        except:
            log_web('Trace not available')

        # Connect to display framebuffer stream
        self.display_sock = None
        self.display_frame = b''
        self.display_w = 320
        self.display_h = 240
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
            self._prev_frame = None
            def ws_push_loop():
                while True:
                    time.sleep(0.03)
                    raw = self.display_frame
                    if not raw: continue
                    prev = self._prev_frame
                    if prev is raw: continue
                    self._prev_frame = raw
                    # Build delta (or full frame if first push or size changed)
                    if prev is not None and len(prev) == len(raw):
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
            threading.Thread(target=ws_push_loop, daemon=True).start()
        except:
            log_web('Display not available')

        # Connect to audio stream
        self.audio_buf = b''
        self._audio_lock = threading.Lock()
        try:
            self.audio_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.audio_sock.connect(('127.0.0.1', AUDIO_PORT))
            log_web(f'Connected to audio port {AUDIO_PORT}')
            def audio_reader():
                while True:
                    try:
                        data = self.audio_sock.recv(8192)
                        if not data: break
                        with self._audio_lock:
                            self.audio_buf += data
                            # Cap buffer at 64KB to avoid unbounded growth
                            if len(self.audio_buf) > 65536:
                                self.audio_buf = self.audio_buf[-65536:]
                    except: break
            threading.Thread(target=audio_reader, daemon=True).start()
        except:
            log_web('Audio not available')

        # Connect to board I/O chardev
        self.io_sock = None
        try:
            self.io_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.io_sock.connect(('127.0.0.1', IO_PORT))
            log_web(f'Connected to I/O port {IO_PORT}')
        except:
            log_web('I/O chardev not available')

    def _ws_encode(self, data):
        """Encode a WebSocket binary frame."""
        b = bytearray([0x82])  # FIN + binary opcode
        n = len(data)
        if n < 126: b.append(n)
        elif n < 65536: b.append(126); b += struct.pack('>H', n)
        else: b.append(127); b += struct.pack('>Q', n)
        return bytes(b) + data

    # ── WebSocket RFC 6455 handshake ──────────────────────────────────
    # Implements the opening handshake per RFC 6455 §4.2.2.
    # The magic GUID must be exactly 258EAFA5-E914-47DA-95CA-C5AB0DC85B11
    # — Chrome validates the Sec-WebSocket-Accept hash and rejects the
    # connection if the GUID is wrong.
    #
    # After upgrade the server sends binary frames (opcode 0x82) with
    # delta-compressed display data:
    #   Frame format: type byte (0=full frame, 1=delta) + payload
    #   Delta format: repeated (uint32_le offset, uint16_le length, data) runs
    # ──────────────────────────────────────────────────────────────────
    def _ws_upgrade(self, conn, headers):
        """Perform WebSocket handshake."""
        key = None
        for line in headers.split('\r\n'):
            if line.lower().startswith('sec-websocket-key:'):
                key = line.split(':', 1)[1].strip()
        if not key: return False
        accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
        resp = (f'HTTP/1.1 101 Switching Protocols\r\n'
                f'Upgrade: websocket\r\nConnection: Upgrade\r\n'
                f'Sec-WebSocket-Accept: {accept}\r\n\r\n')
        conn.sendall(resp.encode())
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
            c = self._ws_status_client
            if c:
                try:
                    c.sendall(self._ws_encode(resp.encode()))
                except:
                    self._ws_status_client = None
        self._ws_status_client = getattr(self, '_ws_status_client', None)
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

                if req.startswith('GET /init'):
                    self.http_response(conn, '200 OK', 'application/json', self.last_state)

                elif req.startswith('GET /ws-uart'):
                    if self._ws_upgrade(conn, data):
                        # Send existing buffer as initial data
                        if self.uart_buf:
                            conn.sendall(self._ws_encode(self.uart_buf.encode()))
                        self._ws_uart_clients.append(conn)
                        continue

                elif req.startswith('GET /ws-display'):
                    if self._ws_upgrade(conn, data):
                        # Send init message with display dimensions
                        ew = self.display_w; eh = self.display_h
                        init = bytes([2]) + struct.pack('<HH', ew, eh)
                        conn.sendall(self._ws_encode(init))
                        with self._ws_lock:
                            self._ws_clients.append(conn)
                        continue  # don't close conn

                elif req.startswith('POST /io'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    if self.io_sock:
                        self.io_sock.sendall((body.strip() + '\n').encode())
                    self.http_response(conn, '200 OK', 'application/json', '{"ok":true}')

                elif req.startswith('POST /gpio'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    try:
                        import json as _json
                        msg = _json.loads(body.strip())
                        port = msg.get('port', 0)
                        pin = msg.get('pin', 0)
                        val = msg.get('val', 0)
                        if self.io_sock:
                            self.io_sock.sendall(f'gpio:{port}:{pin}:{val}\n'.encode())
                    except: pass
                    self.http_response(conn, '200 OK', 'application/json', '{"ok":true}')

                elif req.startswith('GET /audio'):
                    with self._audio_lock:
                        data = self.audio_buf
                        self.audio_buf = b''
                    self.http_response(conn, '200 OK', 'application/octet-stream', data)

                elif req.startswith('GET /ws-trace'):
                    if self._ws_upgrade(conn, data):
                        import json as _json
                        # Send existing events as initial batch
                        for evt in self.trace_events[-512:]:
                            try: conn.sendall(self._ws_encode(_json.dumps(evt).encode()))
                            except: break
                        self._ws_trace_clients.append(conn)
                        continue

                elif req.startswith('GET /ws-status'):
                    if self._ws_upgrade(conn, data):
                        self._ws_status_client = conn
                        continue

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
