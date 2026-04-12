# ClawGlance S3

A physical mission control dashboard for [OpenClaw](https://openclaw.ai) agents, built on an ESP32-S3 with a 3.5" touchscreen display.

Turn your autonomous AI agent from a "black box running in the background" into something you can trust, monitor, and safely operate in real time.

## What It Does

**Dashboard** — glanceable status at arm's length:
- Agent status (IDLE / ACTIVE / ERROR) with session name and duration
- Context usage bar with percentage and token counts
- Three KPI cards: cost today (with burn rate), tokens consumed, active sessions
- Model name and OpenAI usage budget bars (5h window + weekly)
- Cache hit rate bar
- Gateway health indicator with heartbeat age
- Color-coded alerts when thresholds are crossed

**Activity Feed** — real-time agent transcript:
- Parsed from OpenClaw session JSONL files
- Shows user messages, tool calls, and agent replies
- Color-coded: green (user), amber (tools), grey (replies)
- Last 10 events, auto-updates every 5 seconds

**Control Screen** — manage your setup:
- Refresh Status / Restart Gateway buttons
- Screen brightness slider (persists across reboots)
- WiFi SSID and password configuration
- Gateway host:port configuration
- On-screen keyboard for text input
- Save & Reboot to apply changes

**About Screen** — swipe right from dashboard:
- OpenClaw lobster logo, versions, system info
- Swipe navigation between all screens

## Hardware

- **ESP32-S3 3.5" Touch Display Board** (~$20 on AliExpress)
  - 480x320 TFT LCD (ST7796S controller, SPI)
  - Capacitive touchscreen
  - 8MB Flash, 8MB PSRAM (octal)
  - USB-C for power and flashing
- That's it. No soldering required.

## Architecture

```
┌─────────────┐     HTTP/5s       ┌─────────────────────────────────┐
│   ESP32-S3  │ ◄───────────────► │        OpenClaw Gateway         │
│  (display)  │    port 18789     │  ┌───────────────────────────┐  │
│             │                   │  │  clawglance plugin        │  │
│             │  /api/clawglance/ │  │    /api/clawglance/*      │  │
│             │                   │  │  (runs in-process: reads  │  │
│             │ ─── /health ────► │  │   session JSONL, tails    │  │
│             │                   │  │   logs, polls CLI every   │  │
│             │                   │  │   3s)                     │  │
└─────────────┘                   │  └───────────────────────────┘  │
                                  └─────────────────────────────────┘
```

- **plugin-clawglance** is a TypeScript gateway plugin that runs inside the OpenClaw gateway process. It polls session files, `openclaw status --json`, `openclaw gateway usage-cost`, and tails gateway/runtime logs every 3 seconds, then serves the result at `/api/clawglance/*` on the gateway's own port.
- **ESP32** polls the plugin over HTTP every 5 seconds and runs a `/health` check directly against the gateway every 30 seconds.
- **Zero token cost** for monitoring — all data comes from local files and CLI output, not LLM calls.
- WiFi and gateway settings are configurable on-device and persist in NVS flash.

## Quick Start

### 1. Install PlatformIO

```bash
pip install platformio
# or
brew install platformio
```

### 2. Configure

Edit `main/config.h` with your defaults, or configure on-device after first flash:

```c
#define CG_WIFI_SSID  "YOUR_WIFI_SSID"
#define CG_WIFI_PASS  "YOUR_WIFI_PASSWORD"
#define CG_OC_HOST    "192.168.1.100"  // your OpenClaw host LAN IP
#define CG_OC_TOKEN   "YOUR_GATEWAY_TOKEN"
```

### 3. Flash

```bash
pio run -t upload
```

If the USB port isn't auto-detected:
```bash
pio run -t upload --upload-port /dev/cu.usbmodemXXXXX
```

### 4. Install the ClawGlance Plugin

On the machine running OpenClaw, drop `plugin-clawglance/` into the OpenClaw extensions directory as `clawglance`, then restart the gateway so it picks up the new plugin:

```bash
# One-time install (copy):
cp -r plugin-clawglance ~/.openclaw/extensions/clawglance

# Or for development — symlink so source edits reload on gateway restart:
ln -s "$(pwd)/plugin-clawglance" ~/.openclaw/extensions/clawglance

# Restart the gateway:
openclaw gateway restart
```

Once loaded, the plugin exposes REST endpoints on the gateway's own port (default `18789`, Bearer-auth with your gateway token):

- `/api/clawglance/telemetry` — model, context, tokens, cache, budget
- `/api/clawglance/costs` — daily cost and token count
- `/api/clawglance/sessions` — session list with status
- `/api/clawglance/transcript` — recent agent transcript from session JSONL
- `/api/clawglance/activity` — gateway log events
- `/api/clawglance/system` — version, model, session counts
- `/api/clawglance/chat` — POST a message into the active session (used by the Send screen)

Note that the plugin requires OpenClaw gateway `>= 2026.3.24-beta.2` (the plugin API version is pinned in `plugin-clawglance/openclaw.plugin.json`).

### 5. Configure OpenClaw Gateway

The gateway must be accessible from your LAN:

```bash
openclaw config set gateway.bind lan
```

## Project Structure

```
clawglance-s3/
├── main/
│   ├── main.c              # Entry point, polling loop, boot sequence
│   ├── config.h            # WiFi, gateway, display settings
│   ├── app_state.h         # Data structures (telemetry, sessions, etc.)
│   ├── ui_screens.c        # LVGL UI — all 4 screens + update functions
│   ├── ui_screens.h        # Public UI API
│   ├── wifi_mgr.c/h        # ESP-IDF WiFi with auto-reconnect
│   ├── oc_client.c/h       # HTTP client for the gateway plugin
│   ├── lobster_icon.c      # Custom LVGL font — lobster emoji (16px)
│   ├── lobster_icon_lg.c   # Large lobster emoji (48px) for about screen
│   └── Kconfig.projbuild   # LVGL task priority config
├── components/             # Pre-compiled board BSP libraries
│   ├── lvgl/               # LVGL 8.3 source
│   ├── lv_port/            # Display + touch driver (ST7796S)
│   ├── lcd_bsp/            # Board support package
│   └── pwm/                # PWM driver
├── plugin-clawglance/      # OpenClaw gateway plugin (TypeScript)
│   ├── index.ts            # Polling + HTTP routes (runs in-process)
│   ├── openclaw.plugin.json
│   └── package.json
├── platformio.ini          # PlatformIO build config
├── CMakeLists.txt          # ESP-IDF CMake config
├── partitions.csv          # Flash partition table
└── sdkconfig.defaults      # ESP-IDF Kconfig defaults
```

## Key Technical Details

- **Internal RAM contention**: The display DMA and WiFi both need internal (non-PSRAM) RAM. WiFi is initialized with reduced buffers (`static_rx=4, dynamic_rx=8, dynamic TX mode`) to coexist with the display driver.
- **ESP-IDF 5.3.x required**: The pre-compiled BSP libraries (`.a` files) were built for ESP-IDF 5.3.x. PlatformIO is pinned to `espressif32@6.8.1` which bundles 5.3.0.
- **LVGL thread safety**: All LVGL updates use `lv_port_sem_take/give` semaphores. The main polling loop runs on `app_main`'s stack (large enough for HTTP+JSON).
- **NVS persistence**: WiFi credentials, gateway config, and brightness are stored in NVS flash and loaded on boot.

## Screens

| Screen | Access | Description |
|--------|--------|-------------|
| **Dashboard** | Default / swipe | Mission control with health, KPIs, context, budget |
| **Activity** | Tab / swipe left | Session transcript feed (user msgs, tool calls, replies) |
| **Control** | Tab / swipe left | Buttons, brightness, WiFi/gateway config |
| **About** | Swipe right from dash | Version info, system details, credits |

## License

MIT

## Credits

Built with [Claude Code](https://claude.ai/claude-code) on the [OpenClaw](https://openclaw.ai) platform.
