#pragma once

// ============================================================
// ClawGlance Configuration — ESP32-S3 3.5" Port
// ============================================================
// These are compile-time defaults. WiFi and gateway settings
// can be changed on the device via the Control screen and are
// saved to NVS flash, persisting across reboots.
// ============================================================

// --- WiFi (defaults, editable on device) ---
#define CG_WIFI_SSID        "YOUR_WIFI_SSID"
#define CG_WIFI_PASS        "YOUR_WIFI_PASSWORD"
#define CG_WIFI_TIMEOUT_MS  15000
#define CG_WIFI_RETRY_MS    5000

// --- OpenClaw Gateway (defaults, editable on device) ---
#define CG_OC_HOST          "192.168.1.100"
#define CG_OC_PORT          18789
#define CG_OC_TOKEN         "YOUR_GATEWAY_TOKEN"
#define CG_OC_AGENT_ID      "main"
#define CG_OC_USE_HTTPS     0

// --- Bridge (bridge.py running on same machine as OpenClaw) ---
#define CG_OC_DASH_PORT     7001
#define CG_SESSIONS_MAX     8
#define CG_DASH_CHECK_MS    300000

// --- Polling ---
#define CG_POLL_INTERVAL_MS 5000
#define CG_HEALTH_INTERVAL  60000

// --- Display (3.5" 480x320) ---
#define CG_SCREEN_WIDTH     480
#define CG_SCREEN_HEIGHT    320

// --- NTP ---
#define CG_NTP_SERVER       "pool.ntp.org"
#define CG_NTP_GMT_OFFSET   28800   // UTC+8 (Singapore). Change for your timezone.
#define CG_NTP_DST_OFFSET   0

// --- Version ---
#define CLAWGLANCE_VERSION  "0.2.0"

// --- Local overrides (gitignored) ---
#if __has_include("config_local.h")
#include "config_local.h"
#endif
