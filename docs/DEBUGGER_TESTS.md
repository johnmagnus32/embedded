Write a comprehensive test suite for the sim-dbg debugger. Tests run against QEMU (not sim-core) to isolate debugger bugs from emulator bugs. Work in `/home/johmagnu/learning/simple-stm32/dbg`. Read all files in `src/` before making changes.

## Architecture

```
dbg/
├── src/           ← debugger source (existing)
├── tests/
│   ├── run_tests.py       ← Python test runner
│   ├── targets/            ← small ARM firmware test programs
│   │   ├── Makefile
│   │   ├── linker.ld
│   │   ├── test_hello.c
│   │   ├── test_breakpoint.c
│   │   ├── test_stepping.c
│   │   ├── test_variables.c
│   │   ├── test_scopes.c
│   │   └── test_stack.c
│   └── conftest.py         ← shared QEMU + RSP helpers (optional)
├── Makefile       ← add `test` target
└── build/
```

Tests use QEMU's GDB stub (`-gdb tcp::PORT -S`) so failures are always debugger bugs, never emulator bugs. Each test target is a minimal ARM Cortex-M3 program compiled with `-Og -g` for full DWARF debug info.

## Test targets

Each target is a small standalone firmware (no OS, no RTOS) with a vector table, reset handler, and the specific code pattern being tested. Use the same toolchain and linker script as `rtos/tests/`.

### test_hello.c
Minimal program that writes to a known memory address and loops. Used to test basic connection, register read, memory read.
```c
volatile int marker = 0;
void main(void) {
    marker = 0xDEADBEEF;
    while (1) {}
}
```

### test_breakpoint.c
Multiple functions at known locations. Tests breakpoint set/hit/remove.
```c
volatile int counter = 0;
void func_a(void) { counter++; }
void func_b(void) { counter += 10; }
void main(void) {
    func_a();
    func_b();
    func_a();
    while (1) {}
}
```

### test_stepping.c
Tests step-in, step-over, step-out with nested function calls.
```c
int add(int a, int b) { return a + b; }
int multiply(int a, int b) {
    int result = 0;
    for (int i = 0; i < b; i++)
        result = add(result, a);
    return result;
}
void main(void) {
    volatile int x = multiply(3, 4);
    volatile int y = add(x, 1);
    while (1) {}
}
```

### test_variables.c
Tests global variables, local variables (register and stack), pointers, structs, arrays.
```c
int g_int = 42;
const char *g_str = "hello";

struct point { int x; int y; };

void main(void) {
    int local_int = 7;
    struct point p = {10, 20};
    int arr[4] = {1, 2, 3, 4};
    struct point *pp = &p;
    volatile int sink = local_int + p.x + arr[2] + pp->y;
    while (1) {}
}
```

### test_scopes.c
Tests variable scoping — same variable name in different scopes, nested blocks.
```c
void main(void) {
    int x = 1;
    {
        int x = 2;
        volatile int sink = x;  /* should be 2 */
    }
    {
        int x = 3;
        volatile int sink = x;  /* should be 3 */
    }
    volatile int sink = x;  /* should be 1 */
    while (1) {}
}
```

### test_stack.c
Tests backtrace with multiple call frames.
```c
volatile int depth = 0;
void func_c(void) { depth++; while (1) {} }
void func_b(void) { func_c(); }
void func_a(void) { func_b(); }
void main(void) { func_a(); }
```

## Test runner (run_tests.py)

Python script that for each test:
1. Compiles the target (if not already built)
2. Starts QEMU with `-machine lm3s6965evb -nographic -kernel <elf> -gdb tcp::<port> -S`
3. Connects sim-dbg (REPL mode) via piped stdin/stdout
4. Sends commands and verifies output
5. Kills QEMU

### RSP helper class

```python
class DebugSession:
    """Manages a QEMU + sim-dbg session for testing."""
    
    def __init__(self, elf_path, port=None):
        self.port = port or find_free_port()
        self.qemu = subprocess.Popen([
            'qemu-system-arm', '-machine', 'lm3s6965evb', '-nographic',
            '-kernel', elf_path, '-gdb', f'tcp::{self.port}', '-S'
        ], stderr=subprocess.PIPE)
        time.sleep(0.3)
        self.dbg = subprocess.Popen([
            DBG_PATH, '--connect', f'localhost:{self.port}', '--elf', elf_path
        ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    def cmd(self, command):
        """Send a command and return the output line."""
        self.dbg.stdin.write((command + '\n').encode())
        self.dbg.stdin.flush()
        return self.dbg.stdout.readline().decode().strip()
    
    def close(self):
        self.dbg.stdin.write(b'quit\n')
        self.dbg.stdin.flush()
        self.dbg.wait(timeout=2)
        self.qemu.kill()
```

## Test cases

### 1. Connection and basic state

```python
def test_connect():
    """sim-dbg connects to QEMU and shows initial stop."""
    s = DebugSession('targets/build/test_hello.elf')
    output = read_until_prompt(s)
    assert 'Stopped:' in output
    assert '0x' in output  # has a PC address
    s.close()

def test_registers():
    """info regs shows all 16 ARM registers + xPSR."""
    s = DebugSession('targets/build/test_hello.elf')
    s.cmd('regs')
    output = read_output(s)
    assert 'r0' in output
    assert 'sp' in output
    assert 'pc' in output
    assert 'xpsr' in output
    s.close()

def test_memory_read():
    """Can read memory at a known address."""
    s = DebugSession('targets/build/test_hello.elf')
    s.cmd('b main')
    s.cmd('c')
    s.cmd('s')  # step past marker = 0xDEADBEEF
    # Read the marker variable
    output = s.cmd('p marker')
    assert '0xdeadbeef' in output.lower() or '3735928559' in output
    s.close()
```

### 2. Breakpoints

```python
def test_breakpoint_by_function():
    """Break on function name, verify PC is at function entry."""
    s = DebugSession('targets/build/test_breakpoint.elf')
    s.cmd('b func_a')
    output = s.cmd('c')
    assert 'func_a' in output
    s.close()

def test_breakpoint_by_line():
    """Break on file:line."""
    s = DebugSession('targets/build/test_breakpoint.elf')
    s.cmd('b test_breakpoint.c:5')
    output = s.cmd('c')
    assert 'test_breakpoint.c' in output or 'func_b' in output
    s.close()

def test_breakpoint_hit_twice():
    """Breakpoint fires on every hit (not just first)."""
    s = DebugSession('targets/build/test_breakpoint.elf')
    s.cmd('b func_a')
    s.cmd('c')  # first hit
    output = s.cmd('c')  # second hit (func_a called twice)
    assert 'func_a' in output
    s.close()

def test_breakpoint_delete():
    """Deleted breakpoint doesn't fire."""
    s = DebugSession('targets/build/test_breakpoint.elf')
    s.cmd('b func_a')
    s.cmd('b func_b')
    s.cmd('c')  # hits func_a
    s.cmd('d 0x...')  # delete func_a breakpoint
    output = s.cmd('c')  # should hit func_b, not func_a again
    assert 'func_b' in output
    s.close()
```

### 3. Stepping

```python
def test_step_in():
    """Step into a function call."""
    s = DebugSession('targets/build/test_stepping.elf')
    s.cmd('b main')
    s.cmd('c')
    s.cmd('s')  # step into multiply()
    output = read_output(s)
    assert 'multiply' in output
    s.close()

def test_step_over():
    """Step over a function call stays in the same function."""
    s = DebugSession('targets/build/test_stepping.elf')
    s.cmd('b main')
    s.cmd('c')
    s.cmd('n')  # step over multiply()
    output = read_output(s)
    assert 'main' in output  # still in main, not inside multiply
    s.close()

def test_step_advances_line():
    """Each step moves to a different source line."""
    s = DebugSession('targets/build/test_stepping.elf')
    s.cmd('b multiply')
    s.cmd('c')
    line1 = get_current_line(s)
    s.cmd('s')
    line2 = get_current_line(s)
    assert line1 != line2
    s.close()
```

### 4. Variables and expressions

```python
def test_global_variable():
    """Can read a global variable."""
    s = DebugSession('targets/build/test_variables.elf')
    s.cmd('b main')
    s.cmd('c')
    output = s.cmd('p g_int')
    assert '42' in output
    s.close()

def test_local_variable():
    """Can read a local variable after it's initialized."""
    s = DebugSession('targets/build/test_variables.elf')
    s.cmd('b test_variables.c:SINK_LINE')  # line with volatile sink
    s.cmd('c')
    output = s.cmd('p local_int')
    assert '7' in output
    s.close()

def test_struct_member():
    """Can read struct.member."""
    s = DebugSession('targets/build/test_variables.elf')
    s.cmd('b test_variables.c:SINK_LINE')
    s.cmd('c')
    output = s.cmd('p p.x')
    assert '10' in output
    s.close()

def test_array_index():
    """Can read array[index]."""
    s = DebugSession('targets/build/test_variables.elf')
    s.cmd('b test_variables.c:SINK_LINE')
    s.cmd('c')
    output = s.cmd('p arr[2]')
    assert '3' in output
    s.close()

def test_pointer_deref():
    """Can read *pointer and pointer->member."""
    s = DebugSession('targets/build/test_variables.elf')
    s.cmd('b test_variables.c:SINK_LINE')
    s.cmd('c')
    output = s.cmd('p pp->y')
    assert '20' in output
    s.close()

def test_register_name():
    """Can print a register by name."""
    s = DebugSession('targets/build/test_variables.elf')
    s.cmd('b main')
    s.cmd('c')
    output = s.cmd('p sp')
    assert '0x' in output
    s.close()
```

### 5. Variable scoping

```python
def test_inner_scope_shadows():
    """Variable in inner scope shadows outer scope."""
    s = DebugSession('targets/build/test_scopes.elf')
    # Break inside first inner block where x=2
    s.cmd('b test_scopes.c:INNER1_LINE')
    s.cmd('c')
    output = s.cmd('p x')
    assert '2' in output
    s.close()

def test_outer_scope_after_block():
    """After inner block exits, outer variable is visible again."""
    s = DebugSession('targets/build/test_scopes.elf')
    # Break after both inner blocks, where x=1
    s.cmd('b test_scopes.c:OUTER_LINE')
    s.cmd('c')
    output = s.cmd('p x')
    assert '1' in output
    s.close()
```

### 6. Stack and backtrace

```python
def test_backtrace_depth():
    """Backtrace shows correct call chain."""
    s = DebugSession('targets/build/test_stack.elf')
    s.cmd('b func_c')
    s.cmd('c')
    output = s.cmd('bt')
    assert 'func_c' in output
    assert 'func_b' in output
    # Note: deeper frames depend on stack unwinding quality
    s.close()

def test_backtrace_at_main():
    """Backtrace at main shows just main."""
    s = DebugSession('targets/build/test_stack.elf')
    s.cmd('b main')
    s.cmd('c')
    output = s.cmd('bt')
    assert 'main' in output
    s.close()
```

### 7. Source listing

```python
def test_list_shows_source():
    """list command shows source code around current line."""
    s = DebugSession('targets/build/test_hello.elf')
    s.cmd('b main')
    s.cmd('c')
    output = s.cmd('l')
    assert 'marker' in output or 'main' in output
    assert '→' in output  # current line indicator
    s.close()
```

### 8. DAP mode

```python
def test_dap_initialize():
    """DAP initialize returns capabilities."""
    s = DAPSession('targets/build/test_hello.elf')
    resp = s.request('initialize', {'adapterID': 'test'})
    assert resp['success']
    assert 'supportsConfigurationDoneRequest' in resp['body']
    s.close()

def test_dap_breakpoint_and_continue():
    """DAP: set breakpoint, continue, verify stopped event."""
    s = DAPSession('targets/build/test_breakpoint.elf')
    s.initialize_and_attach()
    s.set_breakpoints('test_breakpoint.c', [LINE_OF_FUNC_A])
    s.request('configurationDone', {})
    event = s.wait_for_event('stopped')
    assert event['body']['reason'] == 'breakpoint'
    s.close()

def test_dap_variables():
    """DAP: variables request returns locals."""
    s = DAPSession('targets/build/test_variables.elf')
    s.initialize_and_attach()
    s.set_breakpoints('test_variables.c', [SINK_LINE])
    s.continue_and_wait()
    frames = s.request('stackTrace', {'threadId': 1})['body']['stackFrames']
    scopes = s.request('scopes', {'frameId': frames[0]['id']})['body']['scopes']
    locals_ref = scopes[0]['variablesReference']
    vars = s.request('variables', {'variablesReference': locals_ref})['body']['variables']
    names = [v['name'] for v in vars]
    assert 'local_int' in names or 'p' in names or 'arr' in names
    s.close()

def test_dap_evaluate():
    """DAP: evaluate expression in debug console."""
    s = DAPSession('targets/build/test_variables.elf')
    s.initialize_and_attach()
    s.set_breakpoints('test_variables.c', [SINK_LINE])
    s.continue_and_wait()
    resp = s.request('evaluate', {'expression': 'g_int'})
    assert '42' in resp['body']['result']
    s.close()

def test_dap_stepping():
    """DAP: next and stepIn produce stopped events."""
    s = DAPSession('targets/build/test_stepping.elf')
    s.initialize_and_attach()
    s.set_breakpoints('test_stepping.c', [MAIN_LINE])
    s.continue_and_wait()
    s.request('next', {'threadId': 1})
    event = s.wait_for_event('stopped')
    assert event['body']['reason'] == 'step'
    s.close()
```

### 9. Edge cases

```python
def test_continue_no_breakpoint():
    """Continue with no breakpoints — program runs forever, Ctrl+C halts."""
    s = DebugSession('targets/build/test_hello.elf')
    s.cmd('c')
    time.sleep(0.5)
    s.send_ctrl_c()
    output = read_output(s)
    assert 'Stopped' in output or '0x' in output
    s.close()

def test_step_at_loop():
    """Stepping in a tight loop advances correctly."""
    s = DebugSession('targets/build/test_stepping.elf')
    s.cmd('b multiply')
    s.cmd('c')
    # Step through the for loop body multiple times
    lines = set()
    for _ in range(5):
        s.cmd('s')
        lines.add(get_current_line(s))
    assert len(lines) >= 2  # should visit at least 2 different lines
    s.close()

def test_disconnect_and_reconnect():
    """After disconnect, QEMU keeps running. Can reconnect."""
    # This tests that the stub handles client disconnect gracefully
    pass  # QEMU-specific: stub stays alive after client disconnects
```

## Build system

### dbg/tests/targets/Makefile
```makefile
TC = /home/johmagnu/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi
CC = $(TC)-gcc
CFLAGS = -mcpu=cortex-m3 -mthumb -nostdlib -ffreestanding -Wall -Og -g
LDFLAGS = -T linker.ld -nostdlib

TARGETS = test_hello test_breakpoint test_stepping test_variables test_scopes test_stack
ELFS = $(addprefix build/,$(addsuffix .elf,$(TARGETS)))

all: $(ELFS)

build/%.elf: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -rf build
```

Use the same `linker.ld` as `rtos/tests/linker.ld`.

### dbg/Makefile addition
```makefile
test: $(BUILD)/sim-dbg
	make -C tests/targets
	python3 tests/run_tests.py
```

## Running

```bash
cd dbg
make test
```

Expected output:
```
Building test targets...
  OK

REPL tests:
  test_connect... PASS
  test_registers... PASS
  test_memory_read... PASS
  test_breakpoint_by_function... PASS
  test_breakpoint_by_line... PASS
  test_breakpoint_hit_twice... PASS
  test_breakpoint_delete... PASS
  test_step_in... PASS
  test_step_over... PASS
  test_step_advances_line... PASS
  test_global_variable... PASS
  test_local_variable... PASS
  test_struct_member... PASS
  test_array_index... PASS
  test_pointer_deref... PASS
  test_register_name... PASS
  test_inner_scope_shadows... PASS
  test_outer_scope_after_block... PASS
  test_backtrace_depth... PASS
  test_backtrace_at_main... PASS
  test_list_shows_source... PASS

DAP tests:
  test_dap_initialize... PASS
  test_dap_breakpoint_and_continue... PASS
  test_dap_variables... PASS
  test_dap_evaluate... PASS
  test_dap_stepping... PASS

Edge cases:
  test_continue_no_breakpoint... PASS
  test_step_at_loop... PASS

28 passed, 0 failed
```
