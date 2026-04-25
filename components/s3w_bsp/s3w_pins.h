// Pin map for the Waveshare ESP32-S3-Touch-LCD-3.5 (ESP32-S3R8 +
// ST7796 + FT6336U). Derived from the official schematic.
//
// Notable quirks:
//  - LCD CS is hard-tied to GND on the board (no ESP GPIO controls it).
//  - LCD reset and touch reset are *both* on EXIO1 of a TCA9554 I/O
//    expander on the I2C bus, not on a direct GPIO.
//  - Touch uses I2C only — its INT pin is on the expander, but the
//    FT6336 can be polled directly so we ignore INT entirely.
#pragma once

#include "config.h"

// ---------- Display SPI (SPI2_HOST) ----------
#define S3W_LCD_HOST           SPI2_HOST
#define S3W_LCD_PIN_SCLK       5
#define S3W_LCD_PIN_MOSI       1
#define S3W_LCD_PIN_MISO       2
#define S3W_LCD_PIN_CS         -1   // tied to GND on the board (10K pulldown)
#define S3W_LCD_PIN_DC         3
#define S3W_LCD_PIN_BL         6    // active HIGH (drives S8050 NPN base)
#define S3W_LCD_BL_ON_LEVEL    1
#define S3W_LCD_CMD_BITS       8
#define S3W_LCD_PARAM_BITS     8

#define S3W_LCD_H_RES          480
#define S3W_LCD_V_RES          320
#define S3W_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

// ---------- Shared I2C bus ----------
#define S3W_I2C_PORT           I2C_NUM_0
#define S3W_I2C_PIN_SDA        8
#define S3W_I2C_PIN_SCL        7
#define S3W_I2C_FREQ_HZ        400000

// ---------- TCA9554 I/O expander ----------
// A0/A1/A2 all tied low → addr 0x20.
#define S3W_TCA9554_ADDR       0x20
// EXIOn → bit n of the expander port.
#define S3W_EXIO_LCD_RST       1   // EXIO1: shared LCD + touch reset (active low)
#define S3W_EXIO_SD_CS         3   // EXIO3: SD card chip-select (kept high)

// ---------- FT6336U capacitive touch ----------
#define S3W_TOUCH_ADDR         0x38
