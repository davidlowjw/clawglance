// XPT2046 resistive touch driver for the CYD (ESP32-2432S028R).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the XPT2046 on SPI3_HOST (VSPI). Must be called after
// cyd_lcd_init() since that claims SPI2_HOST.
void cyd_touch_init(void);

// Read touch state. Returns true if touched. x/y are in display
// coordinates (0..319, 0..239 for landscape).
bool cyd_touch_read(int16_t *x, int16_t *y);

// Register as an LVGL pointer input device. Call after lvgl_init().
void cyd_touch_lvgl_register(void);

#ifdef __cplusplus
}
#endif
