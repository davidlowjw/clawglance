/**
 * @file lv_conf.h  — PC simulator configuration
 *
 * Minimal overrides for building ui_screens.c on the host via a dummy
 * display driver. Everything not overridden uses LVGL's internal defaults.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ── Color depth ──────────────────────────────────────────────
 * 32-bit ARGB8888 so we can dump the framebuffer straight to PNG
 * without format conversion. The device runs 16-bit; this only
 * affects the sim's internal buffer. */
#define LV_COLOR_DEPTH 32
#define LV_COLOR_16_SWAP 0  /* only valid when depth==16 */

/* ── Memory ───────────────────────────────────────────────────
 * Four screens + widgets + fonts need more than the default 48KB. */
#define LV_MEM_SIZE (512U * 1024U)

/* ── Tick ─────────────────────────────────────────────────────
 * We drive the tick manually from sim_main via lv_tick_inc. */
#define LV_TICK_CUSTOM 0

/* ── Fonts ────────────────────────────────────────────────────
 * ui_screens.c references Montserrat 12/14/16. Template defaults
 * only have 14 enabled — turn the other two on. */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ── Theme ────────────────────────────────────────────────────
 * Dark theme so elevated/border colors look right against #080808. */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

/* ── Logging ──────────────────────────────────────────────────
 * Off to keep the sim run quiet. */
#define LV_USE_LOG 0

/* ── Perf monitor ─────────────────────────────────────────────
 * Disable the "FPS / CPU" overlay in the bottom-right (LVGL's
 * internal default is 1 when Kconfig is unset). Same fix as
 * the device's components/lvgl/CMakeLists.txt. */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* ── Asserts ──────────────────────────────────────────────────
 * Enabled so we catch layout issues during iteration. */
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE     0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0

/* ── Widgets ──────────────────────────────────────────────────
 * ui_screens.c uses: label, bar, btn, img, keyboard, textarea,
 * slider, switch. All are default-on but we pin them here in
 * case template defaults change. */
#define LV_USE_LABEL      1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_IMG        1
#define LV_USE_KEYBOARD   1
#define LV_USE_TEXTAREA   1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1

/* ── Snapshot / screenshot ────────────────────────────────────
 * Not strictly required for our approach (we read the dummy
 * framebuffer directly), but handy if we ever want per-widget
 * screenshots. */
#define LV_USE_SNAPSHOT 1

#endif /* LV_CONF_H */
