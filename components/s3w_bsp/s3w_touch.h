// FT6336U capacitive touch driver for the Waveshare 3.5" board.
// Polled over the shared I2C bus — INT line goes to the I/O expander
// which would require an extra register read per cycle, so we just
// poll the FT6336 directly. That's the standard pattern for cap touch
// and works fine at LVGL's default 30 Hz read rate.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Bring up the touch IC. Assumes I2C bus + reset line are already up
// (s3w_bsp.c handles both before this is called).
void s3w_touch_init(void);

// Register an LVGL pointer indev that polls the FT6336 each cycle.
void s3w_touch_lvgl_register(void);

#ifdef __cplusplus
}
#endif
