#pragma once

// ============================================================
// ClawGlance Configuration
// ============================================================
// Board selection — set via Kconfig (menu "ClawGlance Board") which
// each sdkconfig.*.defaults file pins.
#if defined(CONFIG_CG_BOARD_CYD_ST7789)
  #define CG_BOARD_CYD_ST7789
  #define CG_BOARD_CYD          // same pins/layout as ILI9341 variant
#elif defined(CONFIG_CG_BOARD_CYD35)
  #define CG_BOARD_CYD35
#elif defined(CONFIG_CG_BOARD_CYD)
  #define CG_BOARD_CYD
#elif defined(CONFIG_CG_BOARD_S3_WAVESHARE)
  #define CG_BOARD_S3_WAVESHARE
#elif defined(CONFIG_CG_BOARD_S3)
  #define CG_BOARD_S3
#else
  // Fallback: auto-detect from IDF target
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    #define CG_BOARD_S3
  #else
    #define CG_BOARD_CYD
  #endif
#endif
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

// --- Sessions ---
#define CG_SESSIONS_MAX     8

// --- Polling ---
#define CG_POLL_INTERVAL_MS 5000
#define CG_HEALTH_INTERVAL  60000

// --- Display ---
#if defined(CG_BOARD_CYD)
  #define CG_SCREEN_WIDTH   320
  #define CG_SCREEN_HEIGHT  240
  #define CG_BL_GPIO        21
#elif defined(CG_BOARD_CYD35)
  #define CG_SCREEN_WIDTH   480
  #define CG_SCREEN_HEIGHT  320
  #define CG_BL_GPIO        27
#elif defined(CG_BOARD_S3_WAVESHARE)
  #define CG_SCREEN_WIDTH   480
  #define CG_SCREEN_HEIGHT  320
  #define CG_BL_GPIO        6
#else  // CG_BOARD_S3
  #define CG_SCREEN_WIDTH   480
  #define CG_SCREEN_HEIGHT  320
  #define CG_BL_GPIO        1
#endif

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
