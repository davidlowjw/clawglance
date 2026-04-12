# ClawGlance PC Simulator

Headless LVGL renderer for the ClawGlance UI. Builds `main/ui_screens.c`
against the vendored LVGL core via a dummy display driver and dumps each
screen to a PNG. Useful for:

- Regenerating README screenshots after UI changes
- Iterating on LVGL layouts without flashing the ESP32
- Verifying that `ui_screens.c` still builds with a non-ESP toolchain

## Build and run

```bash
cd sim
make           # build and run — writes PNGs to sim/out/
make clean
```

Requires a C compiler (clang or gcc). **No external dependencies** — SDL is
not used, PNG writing is via the bundled `stb_image_write.h` (public domain).

## What it renders

Three PNGs into `sim/out/` (and mirrored in `docs/screenshots/` when the
README images are updated):

| File | Screen |
|---|---|
| `01-dashboard.png` | Main mission control view |
| `02-activity.png`  | Session transcript feed |
| `03-control.png`   | /Status, Restart GW, brightness, WiFi config |

The About screen is accessed by swipe gesture on the device and isn't in
the `screen_t` enum, so it's not rendered here.

## How it works

`sim_main.c` initialises LVGL with a **dummy flush callback** that writes
into an in-memory 480×320 ARGB8888 framebuffer (no SDL window). It then
calls the same `ui_build_all()` the device uses, feeds hardcoded plausible
data through `dash_update_*()` / `activity_update()`, pumps ticks past the
screen-load fade animation, and writes each framebuffer to PNG via
`stb_image_write`.

The ESP-IDF-specific bits of `ui_screens.c` (NVS, LEDC backlight PWM,
`esp_restart`) are guarded with `#ifndef CG_SIM` and compile out when
`-DCG_SIM` is set by the sim Makefile. Device behaviour is unchanged.

## Updating screenshots in the README

```bash
cd sim
make
cp out/*.png ../docs/screenshots/
git add ../docs/screenshots/
```

## Limitations

- **Fonts**: Montserrat 12/14/16 are enabled in `sim/lv_conf.h` to match
  the device's `lv_conf`. Letter spacing and kerning should be pixel-identical.
- **Color depth**: sim uses `LV_COLOR_DEPTH=32` for easy PNG output; the
  device runs 16-bit. Colors may differ very slightly on gradients.
- **No input devices**: touch/mouse aren't registered since we only render
  static screens. Swipe-only screens (About) can't be reached without
  exposing `scr_about` in `ui_screens.h`.
