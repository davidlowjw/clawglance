---
name: serial-monitor
description: Headless serial monitor for ESP32 boards via tools/serial_monitor.py. Use when reading serial output from a flashed board during ESP-IDF/PlatformIO debugging, capturing boot logs, or any time `pio device monitor` fails with `termios.error: Operation not supported by device` (i.e. non-TTY contexts — backgrounded shells, agentic harnesses, CI). Auto-detects /dev/cu.usbmodem* and /dev/cu.usbserial-*.
---

# Serial Monitor (ESP32 / ESP-IDF)

Headless serial monitoring via `tools/serial_monitor.py`. Use this
**instead of** `pio device monitor` whenever stdin isn't a real TTY
(backgrounded shell, agentic harness, CI). `pio device monitor` calls
`termios.tcgetattr()` and dies immediately with `Operation not
supported by device` in those contexts.

## Usage

```sh
tools/serial_monitor.py                          # autodetect, stream forever
tools/serial_monitor.py /dev/cu.usbmodem101      # explicit port
tools/serial_monitor.py --duration 8 --reset     # 8-sec snapshot from boot
tools/serial_monitor.py --strip-ansi             # plain text, no colour codes
```

Auto-detects `/dev/cu.usbmodem*` (native USB; ESP32-S3, ESP32-C3) and
`/dev/cu.usbserial-*` (CH340/CP2102; classic ESP32). Errors when more
than one matches — pass the path explicitly in that case.

## Patterns

**Streaming during a debug session** — run in the background, write to
a log file, then `grep`/`tail` the log as needed:

```sh
tools/serial_monitor.py > /tmp/serial.log 2>&1 &
```

**Boot snapshot** — reset the board and capture a fixed window. Useful
when you only care about the first few seconds (init logs, panic
backtraces) and want a script that exits on its own:

```sh
tools/serial_monitor.py --duration 8 --reset --strip-ansi
```

**Filtered streaming** — pipe through `grep --line-buffered` to keep
notifications selective when streaming via the Monitor tool:

```sh
tools/serial_monitor.py 2>&1 | grep --line-buffered -E "panic|^E \(|^W \("
```

## Gotchas

- The port can only be open by one process at a time. If a flash is
  about to run, stop the monitor first (`TaskStop` / `kill`); if a
  monitor is failing to open the port, check whether a previous
  backgrounded reader is still alive.
- After `pio run -t upload` finishes, the board does an `RTS` reset
  automatically — there's a short window where the port enumerates
  again. Wait ~500ms before reopening.
- `--reset` works on dev boards with the standard auto-reset circuit
  (DTR/RTS to EN/IO0). Bare modules without that circuitry need a
  physical reset.
