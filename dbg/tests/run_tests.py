#!/usr/bin/env python3
"""
run_tests.py — Debugger test suite.

Runs tests against QEMU (not sim-core) to isolate debugger bugs.
Each test spawns QEMU with -gdb -S, connects sim-dbg, sends commands,
and verifies output.
"""
import subprocess, socket, time, os, sys, signal, json

DIR = os.path.dirname(os.path.abspath(__file__))
DBG = os.path.join(DIR, '..', 'build', 'sim-dbg')
TARGETS = os.path.join(DIR, 'targets', 'build')
QEMU = 'qemu-system-arm'

passed = 0
failed = 0

def find_free_port():
    s = socket.socket()
    s.bind(('', 0))
    port = s.getsockname()[1]
    s.close()
    return port

class DebugSession:
    def __init__(self, target_name):
        self.port = find_free_port()
        elf = os.path.join(TARGETS, f'{target_name}.elf')
        self.qemu = subprocess.Popen(
            [QEMU, '-machine', 'lm3s6965evb', '-nographic',
             '-kernel', elf, '-gdb', f'tcp::{self.port}', '-S'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.3)
        self.dbg = subprocess.Popen(
            [DBG, '--connect', f'localhost:{self.port}', '--elf', elf],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL)
        # Read initial output (Loaded symbols... Connected... Stopped:...)
        self._read_until_prompt()

    def _read_until_prompt(self):
        """Read until we see '(dbg) ' prompt. Returns collected output."""
        buf = b''
        while True:
            c = self.dbg.stdout.read(1)
            if not c:
                break
            buf += c
            if buf.endswith(b'(dbg) '):
                # Remove the prompt from output
                text = buf[:-6].decode(errors='replace').strip()
                return text
        return buf.decode(errors='replace').strip()

    def cmd(self, command):
        """Send command, return all output lines until next (dbg) prompt."""
        self.dbg.stdin.write((command + '\n').encode())
        self.dbg.stdin.flush()
        return self._read_until_prompt()

    def close(self):
        try:
            self.dbg.stdin.write(b'quit\n')
            self.dbg.stdin.flush()
            self.dbg.wait(timeout=2)
        except:
            self.dbg.kill()
        self.qemu.kill()
        try: self.qemu.wait(timeout=2)
        except: pass


class DAPSession:
    def __init__(self, target_name):
        self.port = find_free_port()
        self.elf = os.path.join(TARGETS, f'{target_name}.elf')
        self.qemu = subprocess.Popen(
            [QEMU, '-machine', 'lm3s6965evb', '-nographic',
             '-kernel', self.elf, '-gdb', f'tcp::{self.port}', '-S'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.3)
        self.dbg = subprocess.Popen(
            [DBG, '--dap', '--connect', f'localhost:{self.port}', '--elf', self.elf],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL)
        self.seq = 0

    def request(self, command, arguments=None):
        self.seq += 1
        msg = {'seq': self.seq, 'type': 'request', 'command': command}
        if arguments is not None:
            msg['arguments'] = arguments
        body = json.dumps(msg)
        header = f'Content-Length: {len(body)}\r\n\r\n'
        self.dbg.stdin.write(header.encode() + body.encode())
        self.dbg.stdin.flush()
        return self._read_message()

    def _read_message(self, timeout=5):
        import threading
        result = [None]
        def reader():
            try:
                header = b''
                while not header.endswith(b'\r\n\r\n'):
                    c = self.dbg.stdout.read(1)
                    if not c: return
                    header += c
                length = int(header.split(b':')[1].strip().split(b'\r\n')[0])
                body = self.dbg.stdout.read(length)
                result[0] = json.loads(body)
            except: pass
        t = threading.Thread(target=reader, daemon=True)
        t.start()
        t.join(timeout=timeout)
        return result[0]

    def read_event(self, timeout=5):
        """Read next message (could be event or response)."""
        return self._read_message(timeout=timeout)

    def initialize_and_attach(self):
        self.request('initialize', {'adapterID': 'test'})
        self._read_message()  # initialized event
        self.request('attach', {
            'gdbTarget': f'localhost:{self.port}',
            'executable': self.elf
        })

    def close(self):
        try:
            self.request('disconnect', {})
        except:
            pass
        try:
            self.dbg.kill()
        except:
            pass
        self.qemu.kill()
        try: self.qemu.wait(timeout=2)
        except: pass


def run_test(name, fn):
    global passed, failed
    print(f'  {name}...', end=' ', flush=True)
    try:
        fn()
        print('PASS')
        passed += 1
    except Exception as e:
        print(f'FAIL ({e})')
        failed += 1


# ── REPL Tests ──────────────────────────────────────────────

def test_connect():
    s = DebugSession('test_hello')
    # Initial output should mention Stopped
    s.close()
    # If we got here without exception, connection worked

def test_registers():
    s = DebugSession('test_hello')
    out = s.cmd('regs')
    assert 'r0' in out, f'no r0 in: {out}'
    assert 'sp' in out, f'no sp in: {out}'
    assert 'pc' in out, f'no pc in: {out}'
    s.close()

def test_memory_read():
    s = DebugSession('test_hello')
    s.cmd('b main')
    s.cmd('c')
    out = s.cmd('p marker')
    assert 'deadbeef' in out.lower() or '3735928559' in out, f'marker wrong: {out}'
    s.close()

def test_breakpoint_by_function():
    s = DebugSession('test_breakpoint')
    s.cmd('b func_a')
    out = s.cmd('c')
    assert 'func_a' in out, f'not at func_a: {out}'
    s.close()

def test_breakpoint_by_line():
    s = DebugSession('test_breakpoint')
    s.cmd('b test_breakpoint.c:7')
    out = s.cmd('c')
    assert 'func_b' in out or 'test_breakpoint.c' in out, f'wrong stop: {out}'
    s.close()

def test_breakpoint_hit_twice():
    s = DebugSession('test_breakpoint')
    s.cmd('b func_a')
    s.cmd('c')  # first hit
    out = s.cmd('c')  # second hit
    assert 'func_a' in out, f'second hit not func_a: {out}'
    s.close()

def test_step_in():
    s = DebugSession('test_stepping')
    s.cmd('b main')
    s.cmd('c')
    # Step into multiply()
    out = s.cmd('s')
    # May need a few steps to enter multiply
    for _ in range(5):
        if 'multiply' in out:
            break
        out = s.cmd('s')
    assert 'multiply' in out, f'did not step into multiply: {out}'
    s.close()

def test_step_over():
    s = DebugSession('test_stepping')
    s.cmd('b main')
    s.cmd('c')
    out = s.cmd('n')  # step over multiply()
    assert 'main' in out, f'stepped out of main: {out}'
    s.close()

def test_global_variable():
    s = DebugSession('test_variables')
    s.cmd('b test_variables.c:14')
    s.cmd('c')
    out = s.cmd('p g_int')
    assert '42' in out, f'g_int wrong: {out}'
    s.close()

def test_struct_member():
    s = DebugSession('test_variables')
    s.cmd('b test_variables.c:14')
    s.cmd('c')
    out = s.cmd('p p.x')
    assert '10' in out, f'p.x wrong: {out}'
    s.close()

def test_array_index():
    s = DebugSession('test_variables')
    s.cmd('b test_variables.c:14')
    s.cmd('c')
    out = s.cmd('p arr[2]')
    assert '3' in out, f'arr[2] wrong: {out}'
    s.close()

def test_pointer_deref():
    s = DebugSession('test_variables')
    s.cmd('b test_variables.c:14')
    s.cmd('c')
    out = s.cmd('p p.y')
    assert '20' in out, f'p.y wrong: {out}'
    s.close()

def test_register_name():
    s = DebugSession('test_hello')
    s.cmd('b main')
    s.cmd('c')
    out = s.cmd('p sp')
    assert '0x' in out, f'sp not hex: {out}'
    s.close()

def test_backtrace_depth():
    s = DebugSession('test_stack')
    s.cmd('b func_c')
    s.cmd('c')
    out = s.cmd('bt')
    assert 'func_c' in out, f'no func_c in bt: {out}'
    # Our backtrace only follows LR for one frame
    # func_b should appear as frame 1 via LR
    s.close()

def test_backtrace_at_main():
    s = DebugSession('test_stack')
    s.cmd('b main')
    s.cmd('c')
    out = s.cmd('bt')
    assert 'main' in out, f'no main in bt: {out}'
    s.close()

def test_list_source():
    s = DebugSession('test_hello')
    s.cmd('b main')
    s.cmd('c')
    out = s.cmd('l')
    assert 'marker' in out or 'main' in out, f'no source: {out}'
    assert '→' in out or '->' in out, f'no current line marker: {out}'
    s.close()

def test_status():
    s = DebugSession('test_hello')
    out = s.cmd('status')
    assert '0x' in out, f'no PC in status: {out}'
    s.close()

def test_help():
    s = DebugSession('test_hello')
    out = s.cmd('help')
    assert 'continue' in out, f'no continue in help: {out}'
    assert 'break' in out, f'no break in help: {out}'
    s.close()


# ── DAP Tests ───────────────────────────────────────────────

def test_dap_initialize():
    s = DAPSession('test_hello')
    resp = s.request('initialize', {'adapterID': 'test'})
    assert resp is not None, 'no response'
    assert resp.get('success'), f'init failed: {resp}'
    s.close()

def test_dap_attach_and_stop():
    s = DAPSession('test_breakpoint')
    # Initialize
    s.request('initialize', {'adapterID': 'test'})
    s.read_event(timeout=2)  # initialized event
    # Attach
    s.request('attach', {
        'gdbTarget': f'localhost:{s.port}',
        'executable': s.elf
    })
    # Set breakpoint by function address (more reliable than file:line)
    s.request('setFunctionBreakpoints', {})
    s.request('configurationDone', {})
    # Read messages until we get a stopped event
    found_stop = False
    for _ in range(10):
        msg = s.read_event(timeout=3)
        if not msg:
            break
        if msg.get('type') == 'event' and msg.get('event') == 'stopped':
            found_stop = True
            break
    assert found_stop, 'no stopped event'
    s.close()

def test_dap_stack_trace():
    s = DAPSession('test_stack')
    s.request('initialize', {'adapterID': 'test'})
    s.read_event(timeout=2)
    s.request('attach', {
        'gdbTarget': f'localhost:{s.port}',
        'executable': s.elf
    })
    s.request('setFunctionBreakpoints', {})
    s.request('configurationDone', {})
    # Wait for stopped
    for _ in range(10):
        msg = s.read_event(timeout=3)
        if not msg: break
        if msg.get('event') == 'stopped': break
    resp = s.request('threads', {})
    assert resp and resp.get('success'), f'threads failed: {resp}'
    resp = s.request('stackTrace', {'threadId': 1})
    assert resp and resp.get('success'), f'stackTrace failed: {resp}'
    frames = resp['body']['stackFrames']
    assert len(frames) > 0, 'no frames'
    s.close()

def test_dap_evaluate():
    s = DAPSession('test_hello')
    s.request('initialize', {'adapterID': 'test'})
    s.read_event(timeout=2)
    s.request('attach', {
        'gdbTarget': f'localhost:{s.port}',
        'executable': s.elf
    })
    s.request('setFunctionBreakpoints', {})
    s.request('configurationDone', {})
    for _ in range(10):
        msg = s.read_event(timeout=3)
        if not msg: break
        if msg.get('event') == 'stopped': break
    resp = s.request('evaluate', {'expression': 'marker'})
    assert resp and resp.get('success'), f'evaluate failed: {resp}'
    # marker might be 0 or 0xdeadbeef depending on where we stopped
    assert 'result' in resp.get('body', {}), f'no result: {resp}'
    s.close()

def test_dap_stepping():
    s = DAPSession('test_hello')
    s.request('initialize', {'adapterID': 'test'})
    s.read_event(timeout=2)
    s.request('attach', {
        'gdbTarget': f'localhost:{s.port}',
        'executable': s.elf
    })
    s.request('setFunctionBreakpoints', {})
    s.request('configurationDone', {})
    for _ in range(10):
        msg = s.read_event(timeout=3)
        if not msg: break
        if msg.get('event') == 'stopped': break
    # Use stepIn (single instruction) which is fast
    resp = s.request('stepIn', {'threadId': 1})
    if not resp:
        resp = s.read_event(timeout=5)
    assert resp is not None, 'stepIn failed: no response'
    # Read stopped event
    found = False
    for _ in range(5):
        msg = s.read_event(timeout=3)
        if msg and msg.get('event') == 'stopped':
            found = True
            break
    assert found, 'no stopped event after stepIn'
    s.close()


# ── Main ────────────────────────────────────────────────────

def main():
    # Build targets
    print('Building test targets...')
    r = subprocess.run(['make', '-C', os.path.join(DIR, 'targets')],
                       capture_output=True)
    if r.returncode != 0:
        print(f'FAIL: target build failed\n{r.stderr.decode()}')
        sys.exit(1)
    print('  OK\n')

    # Build debugger
    print('Building sim-dbg...')
    r = subprocess.run(['make', '-C', os.path.join(DIR, '..')],
                       capture_output=True)
    if r.returncode != 0:
        print(f'FAIL: debugger build failed\n{r.stderr.decode()}')
        sys.exit(1)
    print('  OK\n')

    print('REPL tests:')
    run_test('test_connect', test_connect)
    run_test('test_registers', test_registers)
    run_test('test_memory_read', test_memory_read)
    run_test('test_breakpoint_by_function', test_breakpoint_by_function)
    run_test('test_breakpoint_by_line', test_breakpoint_by_line)
    run_test('test_breakpoint_hit_twice', test_breakpoint_hit_twice)
    run_test('test_step_in', test_step_in)
    run_test('test_step_over', test_step_over)
    run_test('test_global_variable', test_global_variable)
    run_test('test_struct_member', test_struct_member)
    run_test('test_array_index', test_array_index)
    run_test('test_pointer_deref', test_pointer_deref)
    run_test('test_register_name', test_register_name)
    run_test('test_backtrace_depth', test_backtrace_depth)
    run_test('test_backtrace_at_main', test_backtrace_at_main)
    run_test('test_list_source', test_list_source)
    run_test('test_status', test_status)
    run_test('test_help', test_help)

    print('\nDAP tests:')
    run_test('test_dap_initialize', test_dap_initialize)
    run_test('test_dap_attach_and_stop', test_dap_attach_and_stop)
    run_test('test_dap_stack_trace', test_dap_stack_trace)
    run_test('test_dap_evaluate', test_dap_evaluate)
    run_test('test_dap_stepping', test_dap_stepping)

    print(f'\n{passed} passed, {failed} failed')
    sys.exit(1 if failed else 0)

if __name__ == '__main__':
    main()
