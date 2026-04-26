#!/usr/bin/env python3
"""Headless serial monitor for ESP-IDF / PlatformIO ESP32 boards.

A drop-in replacement for `pio device monitor` that does NOT require a
TTY — useful when running in a non-interactive context (backgrounded
shell, CI, agentic harness). `pio device monitor` calls
`termios.tcgetattr()` on stdin and aborts with "Operation not supported
by device" the moment stdin isn't a real terminal; this script reads
the serial port directly via pyserial.

Auto-detects either family of USB serial port:
  /dev/cu.usbmodem*    — native USB (ESP32-S3, ESP32-C3)
  /dev/cu.usbserial-*  — CH340 / CP2102 (classic ESP32 dev kits)

Examples:
    serial_monitor.py                         # autodetect, stream forever
    serial_monitor.py /dev/cu.usbmodem101     # explicit port
    serial_monitor.py --duration 8 --reset    # 8-sec snapshot from boot
    serial_monitor.py --strip-ansi            # plain text, no colour codes
"""

import argparse
import glob
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("error: pyserial not installed (try: pip install pyserial)")


ANSI_RE = re.compile(rb"\x1b\[[0-9;]*m")


def find_port():
    """Return the unique USB serial port, or exit if zero or multiple match."""
    candidates = sorted(
        glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial-*")
    )
    # Filter out common false positives that aren't ESP boards.
    candidates = [
        c for c in candidates
        if "Bluetooth" not in c and "debug-console" not in c
    ]
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        sys.exit("error: no /dev/cu.usbmodem* or /dev/cu.usbserial-* found")
    print("error: multiple serial ports found, pass one explicitly:", file=sys.stderr)
    for c in candidates:
        print(f"  {c}", file=sys.stderr)
    sys.exit(1)


def reset_board(s):
    """Pulse RTS to trigger the auto-reset circuit on common ESP32 dev kits."""
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("port", nargs="?", help="serial port (autodetected if omitted)")
    p.add_argument("--baud", type=int, default=115200,
                   help="baud rate (default 115200)")
    p.add_argument("--duration", type=float, default=0.0,
                   help="exit after N seconds (default: stream until killed)")
    p.add_argument("--reset", action="store_true",
                   help="pulse RTS at startup to reset the board")
    p.add_argument("--strip-ansi", action="store_true",
                   help="strip ANSI colour codes from ESP-IDF logs")
    args = p.parse_args()

    port = args.port or find_port()
    s = serial.Serial(port, args.baud, timeout=1)
    if args.reset:
        reset_board(s)

    deadline = time.time() + args.duration if args.duration else None
    out = sys.stdout.buffer
    try:
        while True:
            chunk = s.read(256)
            if chunk:
                if args.strip_ansi:
                    chunk = ANSI_RE.sub(b"", chunk)
                out.write(chunk)
                out.flush()
            if deadline and time.time() >= deadline:
                break
    except KeyboardInterrupt:
        pass
    finally:
        s.close()


if __name__ == "__main__":
    main()
