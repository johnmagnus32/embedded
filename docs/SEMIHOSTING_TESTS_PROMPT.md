Add semihosting support to the emulator and migrate the test suite from UART-based pass/fail to semihosting exit codes. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/arch/armv7m/armv7m_cpu.c`, `tests/run_tests.py`, `tests/firmware/test.h`, and a few test `.c` files before making changes. Build with `make` from `sim/`.

## Why

The current tests use UART output for pass/fail (`PASS:xxx` / `FAIL:xxx`). This means:
- Every test depends on the UART model being correct
- The runner needs TCP ports, socket connections, and string parsing per test
- A UART bug breaks all tests, even CPU instruction tests

Semihosting lets test firmware signal pass/fail directly to the emulator via a special `BKPT #0xAB` instruction. No UART, no chardevs, no sockets. The emulator exits with the firmware's exit code.

Note: UART-specific tests that verify chardev output will be added separately. This change is only about making the CPU/instruction tests independent of peripherals.

## Part 1: Add semihosting to the CPU emulator

In `src/arch/armv7m/armv7m_cpu.c`, detect `BKPT #0xAB` (opcode `0xBEAB`) and handle semihosting calls:

```c
// In armv7m_cpu_step, in the instruction decoder:
if (insn == 0xBEAB) {
    // ARM semihosting call
    return armv7m_semihosting(cpu, bus);
}
```

Create the semihosting handler (can be in the same file or a new `armv7m_semihost.c`):

```c
#define SYS_OPEN    0x01
#define SYS_CLOSE   0x02
#define SYS_WRITEC  0x03
#define SYS_WRITE0  0x04
#define SYS_WRITE   0x05
#define SYS_READ    0x06
#define SYS_READC   0x07
#define SYS_SEEK    0x0A
#define SYS_FLEN    0x0C
#define SYS_CLOCK   0x10
#define SYS_TIME    0x11
#define SYS_EXIT    0x18
#define SYS_EXIT_EXTENDED 0x20
#define SYS_ELAPSED 0x30
#define SYS_TICKFREQ 0x31

int armv7m_semihosting(struct armv7m_cpu *cpu, struct membus *bus)
{
    uint32_t func = cpu->r[0];
    uint32_t arg  = cpu->r[1];

    switch (func) {
    case SYS_WRITEC: {
        // Write single character (arg = pointer to char)
        char c = membus_read8(bus, arg);
        fputc(c, stderr);
        break;
    }
    case SYS_WRITE0: {
        // Write null-terminated string (arg = pointer to string)
        uint32_t addr = arg;
        char c;
        while ((c = membus_read8(bus, addr++)) != '\0')
            fputc(c, stderr);
        break;
    }
    case SYS_EXIT: {
        // Exit emulation (arg = exit code, or pointer to struct with code)
        // ARM spec: arg is a pointer to a 2-word struct {reason, code}
        // But many implementations just use arg directly as the code
        // Support both: if arg looks like a RAM/flash address, dereference it
        uint32_t code = arg;
        if (arg >= RAM_BASE && arg < RAM_BASE + RAM_SIZE) {
            // arg is a pointer to {reason, code}
            code = membus_read32(bus, arg + 4);
        } else if (arg == 0x20026) {
            // ADP_Stopped_ApplicationExit — success
            code = 0;
        }
        cpu->r[REG_PC] += 2;  // advance past BKPT
        return CPU_SEMIHOST_EXIT | (code & 0xFF);
    }
    case SYS_CLOCK: {
        // Return centiseconds since emulation started
        cpu->r[0] = (uint32_t)(cpu->cycle_count / 160000);  // approx at 16MHz
        break;
    }
    default:
        // Unsupported — return -1
        cpu->r[0] = (uint32_t)-1;
        break;
    }

    cpu->r[REG_PC] += 2;  // advance past BKPT
    return CPU_OK;
}
```

Add a new return code for semihosting exit:

```c
#define CPU_OK              0
#define CPU_BREAKPOINT      1
#define CPU_SEMIHOST_EXIT   0x100  // OR'd with exit code in low byte
```

## Part 2: Handle semihosting exit in the main loop

In sim-core's main loop and bench mode, check for the semihosting exit return:

```c
int result = mach->tick(board);
if (result & CPU_SEMIHOST_EXIT) {
    int code = result & 0xFF;
    exit(code);
}
```

In bench mode, also exit on semihosting:

```c
for (int i = 0; i < bench_cycles; i++) {
    int result = mach->tick(board);
    if (result & CPU_SEMIHOST_EXIT)
        exit(result & 0xFF);
}
```

The `mach->tick` return value needs to propagate from `armv7m_cpu_step` through `stm32f411_tick` and `gameboy_tick`. Update these to return the cpu_step result:

```c
int stm32f411_tick(struct stm32f411 *soc) {
    int r = armv7m_cpu_step(&soc->cpu, &soc->bus);
    armv7m_systick_tick(...);
    armv7m_nvic_update(...);
    return r;
}

int gameboy_tick(struct gameboy *b) {
    return stm32f411_tick(&b->soc);
}
```

Update the `machine_desc` tick function pointer signature to return `int`.

## Part 3: Update test.h to use semihosting

```c
#ifndef TEST_H
#define TEST_H

/* Semihosting calls */
static inline void semi_exit(int code)
{
    register int r0 __asm__("r0") = 0x18;  /* SYS_EXIT */
    register int r1 __asm__("r1") = code;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}

static inline void semi_putc(char c)
{
    register int r0 __asm__("r0") = 0x03;  /* SYS_WRITEC */
    register int r1 __asm__("r1") = (int)&c;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}

static inline void semi_puts(const char *s)
{
    register int r0 __asm__("r0") = 0x04;  /* SYS_WRITE0 */
    register int r1 __asm__("r1") = (int)s;
    __asm volatile("bkpt #0xAB" :: "r"(r0), "r"(r1));
}

static int t_fail_count;
static const char *t_current;

#define CHECK(cond) do { \
    if (!(cond)) { \
        semi_puts("FAIL:"); semi_puts(t_current); \
        semi_puts(":"); semi_puts(#cond); semi_puts("\n"); \
        t_fail_count++; \
    } \
} while (0)

#define TEST(name) t_current = name

#define TEST_DONE(suite) do { \
    if (t_fail_count == 0) { \
        semi_puts("PASS:"); semi_puts(suite); semi_puts("\n"); \
        semi_exit(0); \
    } else { \
        semi_puts("FAILED:"); semi_puts(suite); semi_puts("\n"); \
        semi_exit(1); \
    } \
} while (0)

/* Vector table + startup boilerplate */
extern unsigned int _stack_top;
void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl test_main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler,
};

#endif
```

Key changes:
- `semi_exit(0)` on pass, `semi_exit(1)` on fail — emulator exits with this code
- `semi_puts` for diagnostic output — goes to emulator's stderr, no UART needed
- Diagnostic messages still print so you can see which CHECK failed

## Part 4: Update test firmware

Update `hello.c` to use semihosting:

```c
#include "test.h"

void test_main(void)
{
    semi_puts("HELLO SIM\n");
    semi_exit(0);
}
```

Update `test_irq.c` to use the shared `test.h` instead of its own copy of the macros. It still needs a custom vector table — add a `#define TEST_CUSTOM_VECTORS` guard in `test.h`:

```c
// In test.h, wrap the vector table in a guard:
#ifndef TEST_CUSTOM_VECTORS
extern unsigned int _stack_top;
void test_main(void);
void __attribute__((naked)) reset_handler(void) { __asm volatile("bl test_main\n b .\n"); }
__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = { (void (*)(void))&_stack_top, reset_handler };
#endif

// In test_irq.c:
#define TEST_CUSTOM_VECTORS  // we define our own vector table with ISR entries
#include "test.h"
// ... custom vector table with systick_handler, pendsv_handler, etc.
```

## Part 5: Simplify run_tests.py

The runner no longer needs TCP ports, UART sockets, or debug connections:

```python
def run_test(name, elf_path, timeout=10):
    print(f"  {name}...", end=" ", flush=True)
    try:
        result = subprocess.run(
            [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path, "--no-chardev"],
            timeout=timeout,
            capture_output=True
        )
        # Diagnostic output (semi_puts) goes to stderr
        if result.returncode == 0:
            print("PASS")
            return True
        else:
            print(f"FAIL (exit code {result.returncode})")
            if result.stderr:
                print(f"    {result.stderr.decode().strip()}")
            return False
    except subprocess.TimeoutExpired:
        print("FAIL (timeout)")
        return False

tests = [
    ("hello_uart",  "hello"),
    ("alu_flags",   "test_alu"),
    ("it_blocks",   "test_it"),
    ("func_calls",  "test_call"),
    ("ldm_stm",     "test_ldm_stm"),
    ("mem_access",   "test_mem"),
    ("shift_logic",  "test_shift"),
    ("alu2",         "test_alu2"),
    ("wide",         "test_wide"),
    ("arith",        "test_arith"),
    ("irq",          "test_irq"),
]

for name, fw in tests:
    run_test(name, os.path.join(FW_DIR, "build", f"{fw}.elf"))
```

No ports, no sockets, no string matching. Just run the binary and check exit code. Diagnostic output (which CHECK failed) is captured from stderr.

## Part 6: Future UART-specific tests

This change does NOT remove the ability to test UART output. UART chardev tests will be added separately as a different test category that explicitly tests the UART → chardev → TCP path. Those tests SHOULD depend on the UART model — that's what they're testing. Example:

```python
# Future: tests/test_uart_chardev.py
def test_uart_output():
    """Verify that firmware UART writes arrive on the chardev TCP socket."""
    # Start sim-core WITH chardevs
    # Connect to UART port
    # Send continue
    # Verify specific bytes arrive
    # This test is SUPPOSED to fail if UART is broken
```

But the CPU instruction tests (test_alu, test_mem, test_irq, etc.) should never depend on UART. That's what this semihosting change achieves.

## Testing

After all changes:
1. `make` in `sim/` — verify sim-core builds with semihosting support
2. `make` in `sim/tests/firmware/` — verify test firmware builds
3. `python3 sim/tests/run_tests.py` — all tests should pass via exit codes
4. Verify diagnostic output appears on stderr when a CHECK fails (temporarily break a test to confirm)
