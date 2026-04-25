// ST7796 panel driver for the Waveshare ESP32-S3-Touch-LCD-3.5.
// Same interface shape as cyd_bsp/cyd_lcd.h so cyd_lvgl.c-style
// LVGL bring-up can be done identically.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bring up SPI bus + panel IO + ST7796 init. Caller must already have
// pulsed the panel reset line (the expander does that, see s3w_bsp.c).
// No completion callback registered yet — set it via s3w_lcd_set_done_cb
// after lv_disp_drv_register.
void s3w_lcd_init(void);

typedef void (*s3w_lcd_flush_done_cb_t)(void *user_ctx);
void s3w_lcd_set_done_cb(s3w_lcd_flush_done_cb_t cb, void *user_ctx);

// Push a rectangle of RGB565 pixels. x_end / y_end inclusive. Returns
// immediately; completion signalled via the done callback.
void s3w_lcd_draw(int x0, int y0, int x1, int y1, const void *color_data);

// Synchronous solid-colour fill. Used as a bring-up sanity check.
void s3w_lcd_fill_solid(uint16_t color);

#ifdef __cplusplus
}
#endif
