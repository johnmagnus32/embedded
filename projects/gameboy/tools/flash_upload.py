#!/usr/bin/env python3
"""
flash_upload.py — Program songs.bin to W25Q128 via UART

Usage:
    python3 tools/flash_upload.py /dev/ttyUSB0 build/songs.bin

Protocol:
    1. Send "FLASH\n" to trigger upload mode
    2. Wait for "READY\n" response
    3. Send 4-byte little-endian total size
    4. Send data in 256-byte pages, wait for '.' ack after each
    5. Wait for "DONE\n"
"""

import sys, struct, time, serial

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <serial_port> <songs.bin>")
        sys.exit(1)

    port = sys.argv[1]
    binfile = sys.argv[2]

    data = open(binfile, "rb").read()
    print(f"File: {binfile} ({len(data)} bytes)")

    ser = serial.Serial(port, 115200, timeout=5)
    time.sleep(1)
    ser.reset_input_buffer()

    # Trigger upload mode
    print("Sending FLASH command...")
    ser.write(b"FLASH\n")

    # Wait for READY
    line = ser.readline().decode().strip()
    if line != "READY":
        print(f"Expected READY, got: {line!r}")
        sys.exit(1)
    print("Device ready.")

    # Send total size
    ser.write(struct.pack("<I", len(data)))

    # Wait for erase to complete (dots followed by WRITING)
    sys.stdout.write("Erasing: ")
    while True:
        c = ser.read(1)
        if c == b".":
            sys.stdout.write(".")
            sys.stdout.flush()
        elif c == b"\n":
            line = ser.readline().decode().strip()
            if line == "WRITING":
                break
    print("\nWriting...")

    # Send data in 256-byte pages
    offset = 0
    while offset < len(data):
        chunk = data[offset:offset+256]
        ser.write(chunk)
        # Wait for ack
        ack = ser.read(1)
        if ack != b".":
            print(f"\nError at offset {offset}: got {ack!r}")
            sys.exit(1)
        offset += len(chunk)
        if offset % 4096 == 0:
            sys.stdout.write(".")
            sys.stdout.flush()

    # Wait for DONE
    print()
    line = ser.readline().decode().strip()
    if line == "DONE":
        print(f"Success! Programmed {len(data)} bytes.")
    else:
        print(f"Unexpected response: {line!r}")

    ser.close()

if __name__ == "__main__":
    main()
