#!/usr/bin/env python3
"""
Web debugger for the ARM Cortex-M4 emulator.

Spawns the sim in --headless mode, bridges commands/state via WebSocket.
All UI in the browser: source code, memory map, registers, UART, debugger prompt.

Usage:
    ./state-visualizer <firmware.elf> [--port 3000] [--console /tmp/uart]
"""

import argparse, asyncio, json, os, signal, subprocess, sys
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading

# ── HTML/JS/CSS for the web UI ──

HTML = r"""<!DOCTYPE html>
<html>
<head>
<title>ARM Debugger</title>
<meta charset="utf-8">
<style>
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family:'SF Mono','Menlo','Consolas',monospace; font-size:12px;
       background:#0d1117; color:#c9d1d9; display:flex; flex-direction:column; height:100vh; }
.top { display:flex; flex:1; overflow:hidden; }
.panel { border:1px solid #30363d; margin:4px; border-radius:6px; overflow:hidden; display:flex; flex-direction:column; }
.panel h2 { background:#161b22; padding:6px 10px; font-size:11px; color:#58a6ff; border-bottom:1px solid #30363d; }
.panel-body { flex:1; overflow-y:auto; padding:8px; }
#source-panel { flex:3; }
#mem-panel { flex:2; }
#bottom { display:flex; height:200px; min-height:100px; }
#uart-panel { flex:1; }
#cmd-panel { flex:1; }
.src-line { white-space:pre; padding:1px 4px; }
.src-line.current { background:#1f3a1f; color:#7ee787; font-weight:bold; }
.src-line .num { color:#484f58; display:inline-block; width:40px; text-align:right; margin-right:8px; }
.src-line.current .num { color:#7ee787; }
.reg { display:inline-block; margin:2px 6px; }
.reg .label { color:#484f58; }
.reg .val { color:#58a6ff; }
.mode { padding:1px 6px; border-radius:3px; font-size:10px; }
.mode.thread { background:#1f3a1f; color:#7ee787; }
.mode.handler { background:#3d1f1f; color:#f85149; }
.task { padding:3px 8px; border-bottom:1px solid #21262d; display:flex; align-items:center; gap:8px; }
.task.active { background:#1f3a1f; }
.task .name { width:60px; font-weight:bold; }
.task .name.active { color:#7ee787; }
.task .bar { flex:1; height:8px; background:#21262d; border-radius:4px; overflow:hidden; }
.task .bar-fill { height:100%; background:#f85149; border-radius:4px; }
.task .sp { color:#58a6ff; font-size:10px; }
.task .usage { color:#484f58; font-size:10px; width:70px; }
#uart { white-space:pre-wrap; word-break:break-all; color:#7ee787; font-size:11px; }
#cmd-input { width:100%; background:#0d1117; border:1px solid #30363d; color:#c9d1d9;
             font-family:inherit; font-size:12px; padding:6px 8px; border-radius:4px; }
#cmd-input:focus { outline:none; border-color:#58a6ff; }
#cmd-log { font-size:11px; color:#484f58; margin-bottom:6px; }
.fn { color:#d2a8ff; }
.addr { color:#58a6ff; }
.file { color:#484f58; font-size:10px; }
.header-bar { background:#161b22; padding:4px 10px; border-bottom:1px solid #30363d;
              display:flex; align-items:center; gap:15px; font-size:11px; }
.cyc { color:#484f58; }
</style>
</head>
<body>
<div class="header-bar">
  <span class="fn" id="hdr-func">—</span>
  <span class="cyc" id="hdr-cycles">cy 0</span>
  <span id="hdr-mode"></span>
  <span>PC <span class="addr" id="hdr-pc">—</span></span>
  <span>PSP <span class="addr" id="hdr-psp">—</span></span>
  <span>MSP <span class="addr" id="hdr-msp">—</span></span>
</div>
<div class="top">
  <div class="panel" id="source-panel">
    <h2>Source</h2>
    <div class="panel-body" id="source"></div>
  </div>
  <div class="panel" id="mem-panel">
    <h2>Memory Map</h2>
    <div class="panel-body" id="memmap"></div>
  </div>
</div>
<div id="bottom">
  <div class="panel" id="uart-panel">
    <h2>UART Console</h2>
    <div class="panel-body" id="uart"></div>
  </div>
  <div class="panel" id="cmd-panel">
    <h2>Debugger</h2>
    <div class="panel-body">
      <div id="cmd-log"></div>
      <input id="cmd-input" placeholder="(dbg) type command..." autofocus>
    </div>
  </div>
</div>

<script>
const ws = new WebSocket(`ws://${location.host}/ws`);
const cmdLog = document.getElementById('cmd-log');
const cmdInput = document.getElementById('cmd-input');

function hex(n) { return '0x' + (n >>> 0).toString(16).padStart(8,'0'); }
function esc(s) { const d=document.createElement('div'); d.textContent=s; return d.innerHTML; }

function render(s) {
  // Header
  document.getElementById('hdr-func').textContent = s.function ? `${s.function}+0x${s.func_offset.toString(16)}` : '???';
  document.getElementById('hdr-cycles').textContent = `cy ${s.cycles.toLocaleString()}`;
  document.getElementById('hdr-pc').textContent = hex(s.pc);
  document.getElementById('hdr-psp').textContent = hex(s.psp);
  document.getElementById('hdr-msp').textContent = hex(s.msp);
  document.getElementById('hdr-mode').innerHTML = `<span class="mode ${s.in_handler?'handler':'thread'}">${s.in_handler?'HANDLER':'THREAD'}</span>`;

  // Source
  const src = document.getElementById('source');
  if (s.source && s.source.lines.length) {
    src.innerHTML = `<div class="file">${esc(s.source.file)}</div>` +
      s.source.lines.map(l =>
        `<div class="src-line ${l.num===s.source.current_line?'current':''}">` +
        `<span class="num">${l.num===s.source.current_line?'→':' '}${l.num}</span>${esc(l.text)}</div>`
      ).join('');
    const cur = src.querySelector('.current');
    if (cur) cur.scrollIntoView({block:'center'});
  } else {
    src.innerHTML = '<div style="color:#484f58;padding:20px">No source — compile with -g</div>';
  }

  // Memory map
  let h = '';
  for (const t of s.tasks) {
    const pct = Math.round(t.stack_used/t.stack_size*100);
    h += `<div class="task ${t.active?'active':''}">` +
         `<div class="name ${t.active?'active':''}">${t.active?'▶ ':'  '}${t.name}</div>` +
         `<div class="sp">${hex(t.sp)}</div>` +
         `<div class="bar"><div class="bar-fill" style="width:${pct}%"></div></div>` +
         `<div class="usage">${t.stack_used}/${t.stack_size}</div></div>`;
  }
  document.getElementById('memmap').innerHTML = h || '<div style="color:#484f58;padding:10px">No tasks</div>';

  // UART
  if (s.uart) document.getElementById('uart').textContent = s.uart;
}

ws.onmessage = (e) => {
  try { render(JSON.parse(e.data)); } catch(ex) {}
};

cmdInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    const cmd = cmdInput.value.trim();
    if (!cmd) return;
    cmdLog.innerHTML += `<div style="color:#c9d1d9">(dbg) ${esc(cmd)}</div>`;
    cmdLog.scrollTop = cmdLog.scrollHeight;
    ws.send(cmd);
    cmdInput.value = '';
  }
});

// Button shortcuts
document.addEventListener('keydown', (e) => {
  if (document.activeElement === cmdInput) return;
  if (e.key === 'F5') { ws.send('c'); e.preventDefault(); }
  if (e.key === 'F10') { ws.send('n'); e.preventDefault(); }
  if (e.key === 'F11') { ws.send('s'); e.preventDefault(); }
});
</script>
</body>
</html>"""

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
    if not key: return False
    accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-5AB5CE108231').encode()).digest()).decode()
    conn.sendall(f'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n'.encode())
    return True

def ws_send(conn, data):
    b = data.encode()
    if len(b) < 126:
        frame = bytes([0x81, len(b)]) + b
    elif len(b) < 65536:
        frame = bytes([0x81, 126]) + struct.pack('>H', len(b)) + b
    else:
        frame = bytes([0x81, 127]) + struct.pack('>Q', len(b)) + b
    try:
        conn.setblocking(True)
        conn.sendall(frame)
        conn.setblocking(False)
    except: raise

def ws_recv(conn):
    try:
        conn.setblocking(True)
        conn.settimeout(0.05)
        hdr = conn.recv(2)
        if len(hdr) < 2: return None
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
        conn.setblocking(False)
        return bytes(b ^ mask[i%4] for i,b in enumerate(data)).decode()
    except socket.timeout:
        conn.setblocking(False)
        return ''
    except:
        return None

# ── Main server ──

import socket, select

class WebDebugger:
    def __init__(self, elf, port, console, extra_args):
        self.port = port
        self.elf = elf
        self.console = console
        self.extra_args = extra_args
        self.sim = None
        self.ws_conn = None
        self.last_state = '{}'

    def start_sim(self):
        # Find sim-core in build/ (two levels up from src/sim-web/)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sim_core = os.path.join(script_dir, '..', '..', 'build', 'sim-core')
        cmd = [sim_core, self.elf, '--headless']
        if self.console:
            cmd += ['--console', self.console]
        cmd += self.extra_args
        self.sim = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, bufsize=0)

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
        print(f'Debugger: http://localhost:{self.port}')
        sys.stderr.write(f'Initial state: {len(self.last_state)} bytes\n')

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

                    if 'GET /ws' in req:
                        if ws_handshake(conn, lines[1:]):
                            conn.setblocking(False)
                            ws_clients.append(conn)
                            sys.stderr.write(f'WS client connected, sending {len(self.last_state)} bytes\n')
                            try: ws_send(conn, self.last_state)
                            except Exception as e: sys.stderr.write(f'WS send error: {e}\n')
                    elif 'GET' in req:
                        resp = f'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: {len(HTML)}\r\n\r\n{HTML}'
                        conn.sendall(resp.encode())
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
                            sys.stderr.write(f'CMD: {msg}\n')
                            self.send_cmd(msg)
                    except:
                        if r in ws_clients: ws_clients.remove(r)
                        try: r.close()
                        except: pass

def main():
    p = argparse.ArgumentParser(description='ARM Cortex-M4 Emulator + Web Debugger')
    p.add_argument('elf', help='Firmware ELF file')
    p.add_argument('--port', type=int, default=0, help='HTTP port (default: auto)')
    p.add_argument('--console', help='UART console fifo path')
    args, extra = p.parse_known_args()

    port = args.port if args.port else find_free_port()
    dbg = WebDebugger(args.elf, port, args.console, extra)
    try:
        dbg.run()
    except KeyboardInterrupt:
        print('\nStopped.')
        if dbg.sim: dbg.sim.kill()

if __name__ == '__main__':
    main()
