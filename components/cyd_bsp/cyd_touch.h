// XPT2046 resistive touch driver for the CYD (ESP32-2432S028R).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the XPT2046 SPI device. Must be called after cyd_lcd_init().
void cyd_touch_init(void);

// Read touch state. Returns true if touched. x/y are in display
// coordinates (0..H_RES-1, 0..V_RES-1).
bool cyd_touch_read(int16_t *x, int16_t *y);

// Register as an LVGL pointer input device. Call after lvgl_init().
void cyd_touch_lvgl_register(void);

// Returns true if valid calibration data exists in NVS.
bool cyd_touch_has_calibration(void);

// Run 2-point crosshair calibration. Draws directly on the LCD,
// waits for user taps, computes mapping, and saves to NVS.
// Call after cyd_lcd_init() and cyd_touch_init(), before LVGL UI.
void cyd_touch_calibrate(void);

#ifdef __cplusplus
}
#endif
