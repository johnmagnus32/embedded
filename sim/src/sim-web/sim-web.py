#!/usr/bin/env python3
"""Web debugger for the ARM Cortex-M4 emulator.

Spawns sim-core, connects via TCP, serves HTML UI.
Browser sends commands via HTTP, sim-web forwards to sim-core over TCP.

GET /       → HTML UI
GET /uart   → UART state from file
POST /cmd   → forward command to sim-core, return response
POST /log   → browser logs printed to terminal
"""

import argparse, json, os, socket, select, subprocess, sys, threading, time

def log_web(msg):
    sys.stderr.write(f'[sim-web] {msg}\n')
    sys.stderr.flush()

_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(_dir, "index.html")) as _f:
    HTML = _f.read()

SIM_PORT = 9001
UART_PORT = 9002

class WebDebugger:
    def __init__(self, elf, dts, port, extra_args):
        self.port = port
        self.elf = elf
        self.dts = dts
        self.extra_args = extra_args
        self.sim = None
        self.sim_sock = None
        self.last_state = '{}'

    def start_sim(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sim_core = os.path.join(script_dir, '..', '..', 'build', 'sim-core')
        cmd = [sim_core, self.elf, self.dts,
               '--debug', str(SIM_PORT),
               '--chardev', f'usart2={UART_PORT}'] + self.extra_args
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

    def _recv_line(self):
        """Read one newline-terminated JSON line from sim-core."""
        while b'\n' not in self._buf:
            chunk = self.sim_sock.recv(65536)
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

                elif req.startswith('GET /uart'):
                    import json as _json
                    self.http_response(conn, '200 OK', 'application/json',
                        _json.dumps({"uart": self.uart_buf}))

                elif req.startswith('POST /cmd'):
                    body = data.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in data else ''
                    cmd = body.strip()
                    log_web(f'CMD: {cmd}')
                    try:
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
    p.add_argument('elf', help='Firmware ELF file')
    p.add_argument('dts', help='Board device tree source file')
    args, extra = p.parse_known_args()

    dbg = WebDebugger(args.elf, args.dts, 3000, extra)
    try:
        dbg.run()
    except KeyboardInterrupt:
        log_web('Stopped.')
        if dbg.sim: dbg.sim.kill()

if __name__ == '__main__':
    main()
