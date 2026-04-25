// Common BSP entry point. Each board's BSP component (cyd_bsp,
// s3w_bsp, …) provides its own implementation of `sys_int()`; main.c
// calls it once at startup to bring up the panel/backlight/I2C/etc.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void sys_int(void);

#ifdef __cplusplus
}
#endif
