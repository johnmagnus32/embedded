#!/usr/bin/env python3
"""Web debugger for the ARM Cortex-M4 emulator.

Spawns sim-core, serves HTML UI, bridges commands/state via HTTP.
GET /       → HTML UI
GET /state  → latest JSON state
POST /cmd   → send command to sim-core, return new state
POST /log   → browser logs printed to terminal
"""

import argparse, json, os, queue, socket, select, struct, subprocess, sys, threading

def log_web(msg):
    sys.stderr.write(f'[sim-web] {msg}\n')
    sys.stderr.flush()

_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_dir, "index.html")) as _f:
    HTML = _f.read()

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

class WebDebugger:
    def __init__(self, elf, port, extra_args):
        self.port = port
        self.elf = elf
        self.extra_args = extra_args
        self.sim = None
        self.last_state = '{}'
        self.state_queue = queue.Queue()
        self.state_lock = threading.Lock()

    def start_sim(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sim_core = os.path.join(script_dir, '..', '..', 'build', 'sim-core')
        cmd = [sim_core, self.elf] + self.extra_args
        self.sim = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=sys.stderr, bufsize=0)

    def send_cmd(self, cmd):
        if self.sim and self.sim.poll() is None:
            self.sim.stdin.write((cmd + '\n').encode())
            self.sim.stdin.flush()

    def drain_queue(self):
        """Drain all queued states, keep the latest."""
        updated = False
        while not self.state_queue.empty():
            self.last_state = self.state_queue.get()
            updated = True
        return updated

    def send_cmd_and_wait(self, cmd):
        """Send command and wait briefly for new state."""
        self.send_cmd(cmd)
        # Wait up to 2s for response
        for _ in range(40):
            import time; time.sleep(0.05)
            if self.drain_queue():
                return self.last_state
        return self.last_state

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

        # Read initial state synchronously
        line = self.sim.stdout.readline()
        if line:
            self.last_state = line.decode().strip()

        # Reader thread: sim-core stdout → queue
        def reader():
            while self.sim and self.sim.poll() is None:
                line = self.sim.stdout.readline()
                if line:
                    self.state_queue.put(line.decode().strip())
        threading.Thread(target=reader, daemon=True).start()

        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(('', self.port))
        srv.listen(5)
        srv.setblocking(False)
        log_web(f'Debugger: http://localhost:{self.port}')
        log_web(f'Initial state: {len(self.last_state)} bytes')

        while True:
            self.drain_queue()
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
                log_web(f'Request: {req[:60]}')

                if req.startswith('GET /state'):
                    self.drain_queue()
                    self.http_response(conn, '200 OK', 'application/json', self.last_state)

                elif req.startswith('POST /cmd'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    cmd = body.strip()
                    log_web(f'CMD: {cmd}')
                    state = self.send_cmd_and_wait(cmd)
                    self.http_response(conn, '200 OK', 'application/json', state)

                elif req.startswith('POST /uart'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    text = body.strip()
                    log_web(f'UART TX: {repr(text)}')
                    self.send_cmd(f'u {text}')
                    self.http_response(conn, '200 OK', 'text/plain', '')

                elif req.startswith('POST /log'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    sys.stderr.write(f'[sim-ui] {body}\n')
                    sys.stderr.flush()
                    self.http_response(conn, '200 OK', 'text/plain', '')

                elif req.startswith('GET'):
                    self.http_response(conn, '200 OK', 'text/html; charset=utf-8', HTML)

                else:
                    conn.close()

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
