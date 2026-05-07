#!/usr/bin/env python3
"""
run_tests.py — Emulator test runner.

Mirrors src/ hierarchy: func/ for correctness, perf/ for timing.
  func/arch/armv7m/  — CPU instruction tests
  func/hw/stm32/     — peripheral correctness
  func/core/         — ELF loader
  perf/arch/armv7m/  — CPU/IRQ timing
  perf/hw/stm32/     — SPI/DMA/I2S throughput
  perf/devices/      — ILI9341/MAX98357A chardev output
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


def gdb_send(sock, data):
    """Send a GDB RSP packet."""
    checksum = sum(data.encode()) & 0xFF
    sock.sendall(f"${data}#{checksum:02x}".encode())

def gdb_recv(sock):
    """Receive a GDB RSP packet. Returns data string."""
    # Skip until $
    while True:
        c = sock.recv(1)
        if not c: return None
        if c == b'$': break
        # Skip + ACK bytes
    # Read until #
    buf = b''
    while True:
        c = sock.recv(1)
        if not c: return None
        if c == b'#': break
        buf += c
    sock.recv(2)  # consume checksum
    sock.sendall(b'+')  # ACK
    return buf.decode()

def gdb_connect_and_continue(port, timeout=5):
    """Connect to GDB stub, wait for initial stop, send continue."""
    deadline = time.time() + timeout
    dbg = None
    while time.time() < deadline:
        try:
            dbg = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            dbg.connect(("127.0.0.1", port))
            break
        except ConnectionRefusedError:
            dbg.close()
            time.sleep(0.05)
    if not dbg:
        return None
    dbg.settimeout(2)
    # GDB sends qSupported first, but our test just needs to continue.
    # Send ? to get stop reason, then c to continue.
    gdb_send(dbg, "?")
    gdb_recv(dbg)  # S05
    gdb_send(dbg, "c")
    return dbg


def build_firmware():
    print("Building test firmware...")
    subprocess.run(["make", "-C", FW_DIR, "clean"], capture_output=True)
    r = subprocess.run(["make", "-C", FW_DIR], capture_output=True)
    if r.returncode != 0:
        print(f"FAIL: firmware build failed\n{r.stderr.decode()}")
        sys.exit(1)
    print("  OK")


def elf(path, name):
    return os.path.join(FW_DIR, "build", path, f"{name}.elf")


def run_headless(name, elf_path, timeout=10):
    print(f"  {name}...", end=" ", flush=True)
    try:
        result = subprocess.run(
            [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path],
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


def run_chardev_test(name, elf_path, chardev_name, chardev_port, duration=2,
                     measure="bytes", min_value=0, frame_size=None):
    """Generic chardev test: run firmware, connect to a chardev, measure output."""
    print(f"  {name}...", end=" ", flush=True)
    debug_port = chardev_port - 1
    proc = subprocess.Popen(
        [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path,
         "--gdb", str(debug_port), "--chardev", f"{chardev_name}={chardev_port}"],
        stderr=subprocess.PIPE,
    )
    try:
        deadline = time.time() + duration + 5
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
        gdb_send(dbg, "?")
        gdb_recv(dbg)
        cd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cd.connect(("127.0.0.1", chardev_port))
        cd.settimeout(1)
        gdb_send(dbg, "c")

        total_bytes = 0
        t_start = time.monotonic()
        while time.monotonic() - t_start < duration:
            try:
                chunk = cd.recv(65536)
                if not chunk:
                    break
                total_bytes += len(chunk)
            except (socket.timeout, ConnectionResetError):
                continue

        elapsed = time.monotonic() - t_start
        cd.close()
        dbg.close()

        if measure == "frames":
            count = total_bytes // frame_size if frame_size else 0
            rate = count / elapsed if elapsed > 0 else 0
            label = f"{count} frames in {elapsed:.1f}s = {rate:.0f} FPS"
        elif measure == "samples":
            count = total_bytes // 4
            rate = count / elapsed if elapsed > 0 else 0
            label = f"{count} samples in {elapsed:.1f}s = {rate:.0f} samples/sec"
        else:
            rate = total_bytes
            label = f"{total_bytes} bytes in {elapsed:.1f}s"

        if rate >= min_value:
            print(f"PASS ({label})")
            return True
        print(f"FAIL ({label}, need {min_value})")
        return False
    finally:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


def run_uart_test(name, elf_path, expected, timeout=5):
    print(f"  {name}...", end=" ", flush=True)
    debug_port, uart_port = 19800, 19801
    proc = subprocess.Popen(
        [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path,
         "--gdb", str(debug_port), "--chardev", f"usart2={uart_port}"],
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
        gdb_send(dbg, "?")
        gdb_recv(dbg)
        uart = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        uart.connect(("127.0.0.1", uart_port))
        uart.settimeout(timeout)
        gdb_send(dbg, "c")
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


def run_gpio_chardev_test(name, elf_path, timeout=5):
    """Test GPIO input via IO chardev triggers EXTI, verified via UART output."""
    print(f"  {name}...", end=" ", flush=True)
    debug_port, io_port, uart_port = 19800, 19801, 19802
    proc = subprocess.Popen(
        [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path,
         "--gdb", str(debug_port),
         "--chardev", f"io={io_port}",
         "--chardev", f"usart2={uart_port}"],
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
        gdb_send(dbg, "?")
        gdb_recv(dbg)
        io = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        io.connect(("127.0.0.1", io_port))
        uart = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        uart.connect(("127.0.0.1", uart_port))
        uart.settimeout(timeout)
        gdb_send(dbg, "c")
        time.sleep(0.2)  # let firmware init
        io.sendall(b"gpio:1:0:1\n")  # GPIOB pin 0 high
        # Wait for semihosting exit
        proc.wait(timeout=timeout)
        rc = proc.returncode
        stderr = proc.stderr.read().decode(errors="replace")
        # Check UART for EXTI0_OK
        uart_data = b""
        try:
            while True:
                chunk = uart.recv(4096)
                if not chunk: break
                uart_data += chunk
        except: pass
        uart_text = uart_data.decode(errors="replace")
        has_uart = "EXTI0_OK" in uart_text
        if rc == 0 and "PASS" in stderr and has_uart:
            print(f"PASS  (semihosting + UART verified)")
            return True
        else:
            if rc != 0 or "PASS" not in stderr:
                print(f"FAIL (exit code {rc})")
            elif not has_uart:
                print(f"FAIL (EXTI0_OK not found on UART)")
            for line in stderr.strip().split("\n"):
                if "FAIL" in line:
                    print(f"    {line}")
            return False
    except subprocess.TimeoutExpired:
        print("FAIL (timeout)")
        proc.kill()
        return False
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try: proc.wait(timeout=2)
            except: proc.kill()


def main():
    build_firmware()
    passed = 0
    failed = 0
    FRAME_SIZE = 4 + 240 * 320 * 2

    # ── Functional tests ──────────────────────────────────────────

    print("\nfunc/arch/armv7m:")
    for name in ["test_alu", "test_alu2", "test_it", "test_call", "test_ldm_stm",
                  "test_mem", "test_shift", "test_wide", "test_arith", "test_misc",
                  "test_ldst_w"]:
        ok = run_headless(name, elf("func/arch/armv7m", name))
        passed += ok; failed += not ok

    print("\nfunc/core:")
    ok = run_headless("test_elf", elf("func/core", "test_elf"))
    passed += ok; failed += not ok

    print("\nfunc/hw/stm32:")
    for name in ["test_irq", "test_it_irq", "test_gpio_exti", "test_syscfg", "test_dma", "test_dma_spi", "test_dma_i2s_kick", "test_w25q128"]:
        ok = run_headless(name, elf("func/hw/stm32", name))
        passed += ok; failed += not ok

    print("\nfunc/hw/stm32 (chardev):")
    ok = run_uart_test("test_uart_chardev",
                       elf("func/hw/stm32", "test_uart_chardev"), "UART_OK\n")
    passed += ok; failed += not ok
    ok = run_gpio_chardev_test("test_gpio_irq_chardev",
                               elf("func/hw/stm32", "test_gpio_irq_chardev"))
    passed += ok; failed += not ok

    # ── Performance tests ─────────────────────────────────────────

    print("\nperf/arch/armv7m:")
    for name in ["test_mips", "test_irq_latency", "test_systick_period"]:
        ok = run_headless(name, elf("perf/arch/armv7m", name))
        passed += ok; failed += not ok

    print("\nperf/hw/stm32:")
    for name in ["test_spi_throughput", "test_membus",
                  "test_spi_fps_full", "test_spi_fps_partial", "test_spi_fps_sustained",
                  "test_dma_fps", "test_dma_fps_full", "test_dma_fps_sustained",
                  "test_i2s_cpu", "test_i2s_dma", "test_w25q128_perf"]:
        ok = run_headless(name, elf("perf/hw/stm32", name))
        passed += ok; failed += not ok

    print("\nperf/devices/ili9341:")
    for name, min_fps in [("test_display_refresh", 50),
                           ("test_display_cpu", 50),
                           ("test_display_dma", 50)]:
        ok = run_chardev_test(name, elf("perf/devices/ili9341", name),
                              "display", 19811, measure="frames",
                              frame_size=FRAME_SIZE, min_value=min_fps)
        passed += ok; failed += not ok

    print("\nperf/devices/max98357a:")
    ok = run_chardev_test("test_audio", elf("perf/devices/max98357a", "test_audio"),
                          "audio", 19821, measure="samples", min_value=15000)
    passed += ok; failed += not ok

    print(f"\n{passed} passed, {failed} failed")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
