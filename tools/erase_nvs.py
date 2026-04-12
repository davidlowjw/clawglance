#!/usr/bin/env python3
"""Erase the NVS partition on a CYD board to force touch recalibration on next boot."""

import sys
import subprocess
import glob

ESPTOOL = glob.glob(
    f"{__import__('os').path.expanduser('~')}/.platformio/packages/tool-esptoolpy*/esptool.py"
)

NVS_OFFSET = "0x9000"
NVS_SIZE = "0x6000"


def find_port():
    """Auto-detect the USB serial port."""
    candidates = glob.glob("/dev/cu.usbserial-*")
    if len(candidates) == 1:
        return candidates[0]
    if len(candidates) > 1:
        print("Multiple serial ports found:")
        for i, p in enumerate(candidates):
            print(f"  [{i}] {p}")
        choice = input("Select port number: ").strip()
        return candidates[int(choice)]
    # fallback
    print("No /dev/cu.usbserial-* found. Provide port as argument.")
    sys.exit(1)


def main():
    if not ESPTOOL:
        print("Error: esptool.py not found in ~/.platformio/packages/")
        sys.exit(1)

    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    print(f"Erasing NVS on {port} (offset={NVS_OFFSET}, size={NVS_SIZE})...")

    result = subprocess.run(
        [sys.executable, ESPTOOL[0], "--port", port, "erase_region", NVS_OFFSET, NVS_SIZE],
        capture_output=False,
    )
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
