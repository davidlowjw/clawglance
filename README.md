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
- Gateway and bridge health indicators with heartbeat age
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
┌─────────────┐     HTTP/5s      ┌──────────────┐     CLI/files     ┌──────────────┐
│   ESP32-S3  │ ◄──────────────► │  bridge.py   │ ◄───────────────► │   OpenClaw   │
│  (display)  │    port 7001     │ (your Mac)   │   status --json   │  (gateway)   │
│             │                  │              │   usage-cost      │              │
│             │ ──── /health ──► │              │   session JSONL   │              │
│             │    port 18789    │              │   gateway.log     │              │
└─────────────┘                  └──────────────┘                   └──────────────┘
```

- **bridge.py** runs on the same machine as OpenClaw, polling CLI commands and reading local files every 3 seconds
- **ESP32** polls the bridge over HTTP every 5 seconds and checks gateway health directly every 30 seconds
- **Zero token cost** for monitoring — all data comes from CLI and file reads, not LLM calls
- WiFi and gateway settings are configurable on-device and persist in NVS flash

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

### 4. Start the Bridge

On the machine running OpenClaw:

```bash
python3 bridge.py &
```

The bridge listens on `0.0.0.0:7001` and exposes:
- `/api/telemetry` — model, context, tokens, cache, budget
- `/api/costs` — daily cost and token count
- `/api/sessions` — active session list
- `/api/transcript` — recent agent activity from session JSONL
- `/api/activity` — gateway log events
- `/v1/chat/completions` — send commands (restart gateway, refresh status)

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
│   ├── oc_client.c/h       # HTTP client for bridge + gateway
│   ├── lobster_icon.c      # Custom LVGL font — lobster emoji (16px)
│   ├── lobster_icon_lg.c   # Large lobster emoji (48px) for about screen
│   └── Kconfig.projbuild   # LVGL task priority config
├── components/             # Pre-compiled board BSP libraries
│   ├── lvgl/               # LVGL 8.3 source
│   ├── lv_port/            # Display + touch driver (ST7796S)
│   ├── lcd_bsp/            # Board support package
│   └── pwm/                # PWM driver
├── bridge.py               # Python HTTP proxy for OpenClaw data
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
