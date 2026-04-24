#!/usr/bin/env python3
"""
Web debugger for the ARM Cortex-M4 emulator.

Spawns the sim in --headless mode, bridges commands/state via WebSocket.
All UI in the browser: source code, memory map, registers, UART, debugger prompt.

Usage:
    ./state-visualizer <firmware.elf> [--port 3000] [--console /tmp/uart]
"""

import argparse, asyncio, json, os, signal, subprocess, sys, time
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading

def log_web(msg):
    sys.stderr.write(f'[sim-web] {msg}\n')
    sys.stderr.flush()

# ── HTML/JS/CSS for the web UI ──

_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_dir, "index.html")) as _f:
    HTML = _f.read()

# ── Simple WebSocket implementation (RFC 6455) ──

import hashlib, base64, struct

def find_free_port(start=3000):
    for port in range(start, start + 100):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.bind(('', port))
            s.close()
            return port
        except OSError:
            continue
    return start

def ws_handshake(conn, headers):
    key = None
    for h in headers:
        if h.lower().startswith('sec-websocket-key:'):
            key = h.split(':',1)[1].strip()
    if not key:
        log_web('WS handshake: no key found')
        return False
    GUID = '258EAFA5-E914-47DA-95CA-5AB5CE108231'
    accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
    response = (
        'HTTP/1.1 101 Switching Protocols\r\n'
        'Upgrade: websocket\r\n'
        'Connection: Upgrade\r\n'
        f'Sec-WebSocket-Accept: {accept}\r\n'
        '\r\n'
    )
    log_web(f'WS handshake response: {repr(response[:120])}')
    conn.sendall(response.encode())
    log_web(f'WS handshake OK: key={key[:8]}... accept={accept[:8]}...')
    return True

def ws_send(conn, data):
    b = data.encode('utf-8')
    if len(b) < 126:
        frame = bytes([0x81, len(b)]) + b
    elif len(b) < 65536:
        frame = bytes([0x81, 126]) + struct.pack('>H', len(b)) + b
    else:
        frame = bytes([0x81, 127]) + struct.pack('>Q', len(b)) + b
    log_web(f'WS frame: {len(frame)} bytes, header={frame[:4].hex()}, payload={len(b)}')
    conn.sendall(frame)

def ws_recv(conn):
    try:
        hdr = conn.recv(2)
        if not hdr: return None
        if len(hdr) < 2: return ''
        opcode = hdr[0] & 0x0F
        if opcode == 8: return None
        masked = hdr[1] & 0x80
        length = hdr[1] & 0x7F
        if length == 126: length = struct.unpack('>H', conn.recv(2))[0]
        elif length == 127: length = struct.unpack('>Q', conn.recv(8))[0]
        mask = conn.recv(4) if masked else b'\x00'*4
        data = b''
        while len(data) < length:
            chunk = conn.recv(length - len(data))
            if not chunk: return None
            data += chunk
        return bytes(b ^ mask[i%4] for i,b in enumerate(data)).decode()
    except socket.timeout:
        return ''
    except BlockingIOError:
        return ''
    except:
        return None

# ── Main server ──

import socket, select

class WebDebugger:
    def __init__(self, elf, port, extra_args):
        self.port = port
        self.elf = elf
        self.extra_args = extra_args
        self.sim = None
        self.ws_conn = None
        self.last_state = '{}'

    def start_sim(self):
        # Find sim-core in build/ (two levels up from src/sim-web/)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sim_core = os.path.join(script_dir, '..', '..', 'build', 'sim-core')
        cmd = [sim_core, self.elf]
        cmd += self.extra_args
        self.sim = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=sys.stderr, bufsize=0)

    def send_cmd(self, cmd):
        if self.sim and self.sim.poll() is None:
            self.sim.stdin.write((cmd + '\n').encode())
            self.sim.stdin.flush()

    def read_state(self):
        if not self.sim: return None
        line = self.sim.stdout.readline()
        if not line: return None
        return line.decode().strip()

    def run(self):
        self.start_sim()

        # Read initial state
        self.last_state = self.read_state() or '{}'

        # Thread to read sim stdout
        import queue
        state_queue = queue.Queue()
        def reader():
            while self.sim and self.sim.poll() is None:
                line = self.sim.stdout.readline()
                if line:
                    state_queue.put(line.decode().strip())
        t = threading.Thread(target=reader, daemon=True)
        t.start()

        # Start HTTP server
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(('', self.port))
        srv.listen(5)
        srv.setblocking(False)
        log_web(f'Debugger: http://localhost:{self.port}')
        log_web(f'Initial state: {len(self.last_state)} bytes')

        ws_clients = []

        while True:
            # Drain state queue
            while not state_queue.empty():
                self.last_state = state_queue.get()
                dead = []
                for c in ws_clients:
                    try: ws_send(c, self.last_state)
                    except: dead.append(c)
                for c in dead:
                    ws_clients.remove(c)
                    try: c.close()
                    except: pass

            rlist = [srv] + ws_clients
            try:
                readable, _, _ = select.select(rlist, [], [], 0.05)
            except:
                break

            for r in readable:
                if r is srv:
                    conn, addr = srv.accept()
                    data = conn.recv(4096).decode(errors='ignore')
                    lines = data.split('\r\n')
                    req = lines[0] if lines else ''
                    log_web(f'Request: {req[:60]}')

                    if 'GET /ws' in req:
                        log_web(f'WS upgrade request from client')
                        if ws_handshake(conn, lines[1:]):
                            conn.settimeout(0.05)
                            ws_clients.append(conn)
                            log_web(f'WS client connected, sending {len(self.last_state)} bytes')
                            try:
                                ws_send(conn, self.last_state)
                                log_web(f'WS initial send OK')
                            except Exception as e:
                                log_web(f'WS initial send FAILED: {e}')
                    elif 'POST /log' in req:
                        # Read body (after headers)
                        body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                        sys.stderr.write(f'[sim-ui] {body}\n')
                        sys.stderr.flush()
                        conn.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nAccess-Control-Allow-Origin: *\r\n\r\n')
                        conn.close()
                    elif 'GET' in req:
                        html_bytes = HTML.encode()
                        resp = f'HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: {len(html_bytes)}\r\n\r\n'
                        conn.sendall(resp.encode() + html_bytes)
                        conn.close()
                    else:
                        conn.close()

                elif r in ws_clients:
                    try:
                        msg = ws_recv(r)
                        if msg is None:
                            ws_clients.remove(r)
                            r.close()
                        elif msg:  # non-empty command
                            log_web(f'CMD: {msg}')
                            self.send_cmd(msg)
                    except:
                        if r in ws_clients: ws_clients.remove(r)
                        try: r.close()
                        except: pass

def main():
    p = argparse.ArgumentParser(description='ARM Cortex-M4 Emulator + Web Debugger')
    p.add_argument('elf', help='Firmware ELF file')
    p.add_argument('--port', type=int, default=0, help='HTTP port (default: auto)')
    args, extra = p.parse_known_args()

    port = args.port if args.port else find_free_port()
    dbg = WebDebugger(args.elf, port, extra)
    try:
        dbg.run()
    except KeyboardInterrupt:
        log_web('Stopped.')
        if dbg.sim: dbg.sim.kill()

if __name__ == '__main__':
    main()
