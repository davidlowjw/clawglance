// CYD ILI9341 panel driver — minimal interface used by cyd_lvgl.c.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize SPI bus, panel IO, and ILI9341 panel. Configures the panel
// for landscape orientation (320 wide × 240 tall, USB on the right).
// No completion callback is registered yet — call cyd_lcd_set_done_cb()
// once LVGL is ready, otherwise the ISR fires into uninitialized state.
void cyd_lcd_init(void);

// Register an SPI-completion callback. Invoked from the SPI ISR after
// each color flush — used by LVGL to call lv_disp_flush_ready().
// Must be called AFTER lv_disp_drv_register so the driver pointer is
// fully populated by the time the first transfer queues up.
typedef void (*cyd_lcd_flush_done_cb_t)(void *user_ctx);
void cyd_lcd_set_done_cb(cyd_lcd_flush_done_cb_t done_cb, void *user_ctx);

// Push a rectangle of RGB565 pixels to the panel. x_end / y_end are
// inclusive. Returns immediately; completion is signalled via the
// done_cb registered in cyd_lcd_init().
void cyd_lcd_draw(int x_start, int y_start, int x_end, int y_end, const void *color_data);

// Synchronously fill the entire screen with one RGB565 colour. Used
// for the bring-up red-screen sanity check before LVGL takes over.
void cyd_lcd_fill_solid(uint16_t color);

#ifdef __cplusplus
}
#endif
