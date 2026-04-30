#!/usr/bin/env python3
"""
run_tests.py — Emulator integration test runner.

CPU/HW tests: headless semihosting exit codes.
UART chardev test: TCP socket verification.
Perf tests: wall-clock assertions via SYS_CLOCK semihosting.
"""

import subprocess
import socket
import time
import sys
import os
import signal

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM_CORE = os.path.join(SIM_DIR, "build", "sim-core")
FW_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "firmware")


def build_firmware():
    print("Building test firmware...")
    subprocess.run(["make", "-C", FW_DIR, "clean"], capture_output=True)
    r = subprocess.run(["make", "-C", FW_DIR], capture_output=True)
    if r.returncode != 0:
        print(f"FAIL: firmware build failed\n{r.stderr.decode()}")
        sys.exit(1)
    print("  OK")


def run_headless(name, elf_path, timeout=10):
    """Run firmware headless, pass/fail by exit code."""
    print(f"  {name}...", end=" ", flush=True)
    try:
        result = subprocess.run(
            [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path, "--headless"],
            timeout=timeout, capture_output=True,
        )
        stderr = result.stderr.decode()
        diag = "\n".join(l for l in stderr.strip().split("\n") if not l.startswith("[") and l.strip())
        if result.returncode == 0:
            print(f"PASS  ({diag.strip()})" if diag.strip() else "PASS")
            return True
        print(f"FAIL (exit code {result.returncode})")
        if diag:
            for line in diag.split("\n"):
                print(f"    {line}")
        return False
    except subprocess.TimeoutExpired:
        print("FAIL (timeout)")
        return False


def run_uart_chardev(name, elf_path, expected, timeout=5):
    """Run firmware with UART chardev, verify expected bytes on TCP socket."""
    print(f"  {name}...", end=" ", flush=True)
    debug_port, uart_port = 19800, 19801
    proc = subprocess.Popen(
        [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path,
         "--debug", str(debug_port), "--chardev", f"usart2={uart_port}"],
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.time() + timeout
        dbg = None
        while time.time() < deadline:
            try:
                dbg = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                dbg.connect(("127.0.0.1", debug_port))
                break
            except ConnectionRefusedError:
                dbg.close()
                time.sleep(0.05)
        if not dbg:
            print("FAIL (could not connect)")
            return False

        dbg.settimeout(2)
        dbg.recv(4096)
        uart = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        uart.connect(("127.0.0.1", uart_port))
        uart.settimeout(timeout)
        dbg.sendall(b'{"cmd":"continue"}\n')

        data = b""
        try:
            while expected.encode() not in data and time.time() < deadline:
                chunk = uart.recv(4096)
                if not chunk:
                    break
                data += chunk
        except (socket.timeout, ConnectionResetError):
            pass
        uart.close()
        dbg.close()

        if expected.encode() in data:
            print("PASS")
            return True
        print(f"FAIL\n    expected: {expected!r}\n    got:      {data!r}")
        return False
    finally:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


def main():
    build_firmware()
    passed = 0
    failed = 0

    cpu_tests = [
        "test_alu", "test_alu2", "test_it", "test_call", "test_ldm_stm",
        "test_mem", "test_shift", "test_wide", "test_arith", "test_misc",
        "test_ldst_w", "test_elf",
    ]
    print("\nCPU tests:")
    for name in cpu_tests:
        ok = run_headless(name, os.path.join(FW_DIR, "build", "cpu", f"{name}.elf"))
        passed += ok; failed += not ok

    hw_tests = ["test_irq", "test_it_irq", "test_gpio_exti", "test_dma"]
    print("\nHW tests:")
    for name in hw_tests:
        ok = run_headless(name, os.path.join(FW_DIR, "build", "hw", f"{name}.elf"))
        passed += ok; failed += not ok

    print("\nHW chardev tests:")
    ok = run_uart_chardev("test_uart_chardev",
                          os.path.join(FW_DIR, "build", "hw", "test_uart_chardev.elf"), "UART_OK\n")
    passed += ok; failed += not ok

    perf_tests = [
        "test_perf_mips", "test_perf_systick", "test_perf_irq_latency",
        "test_perf_spi", "test_perf_membus",
    ]
    print("\nPerformance tests:")
    for name in perf_tests:
        ok = run_headless(name, os.path.join(FW_DIR, "build", "hw", f"{name}.elf"))
        passed += ok; failed += not ok

    print(f"\n{passed} passed, {failed} failed")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
