// Pin map for CYD boards (ESP32-2432S028R and ESP32-2432S035).
// SPI pins are identical across both boards; BL and resolution differ.
#pragma once

#include "config.h"

// ---------- Display SPI: SPI2_HOST (HSPI) ----------
#define CYD_LCD_HOST           SPI2_HOST
#define CYD_LCD_PIN_SCLK       14
#define CYD_LCD_PIN_MOSI       13
#define CYD_LCD_PIN_MISO       12
#define CYD_LCD_PIN_CS         15
#define CYD_LCD_PIN_DC          2
#define CYD_LCD_PIN_RST        -1   // tied to EN on both boards
#define CYD_LCD_BL_ON_LEVEL     1
#define CYD_LCD_CMD_BITS        8
#define CYD_LCD_PARAM_BITS      8

#ifdef CG_BOARD_CYD35
  // ESP32-2432S035: ST7796U, 480x320, BL=27
  #define CYD_LCD_PIN_BL         27
  #define CYD_LCD_H_RES         480
  #define CYD_LCD_V_RES         320
  #define CYD_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#else
  // ESP32-2432S028R: ILI9341, 320x240, BL=21
  #define CYD_LCD_PIN_BL         21
  #define CYD_LCD_H_RES         320
  #define CYD_LCD_V_RES         240
  #define CYD_LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#endif

// ---------- Touch: XPT2046, SPI3_HOST (VSPI) ----------
// Same pins on both boards. MISO=39 and IRQ=36 are input-only GPIOs.
#define CYD_TOUCH_HOST         SPI3_HOST
#define CYD_TOUCH_PIN_SCLK     25
#define CYD_TOUCH_PIN_MOSI     32
#define CYD_TOUCH_PIN_MISO     39
#define CYD_TOUCH_PIN_CS       33
#define CYD_TOUCH_PIN_IRQ      36
