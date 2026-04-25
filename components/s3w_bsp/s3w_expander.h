// TCA9554 8-bit I2C I/O expander driver — just enough for what the
// Waveshare board needs (configure a pin as output, drive it high or
// low). The expander shares the system I2C bus with the touch IC,
// codec, RTC, IMU, and PMIC.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the I2C bus driver (idempotent — safe to call from
// multiple init paths). Must be called before any other expander or
// touch I2C calls.
esp_err_t s3w_i2c_init(void);

// Bring up the expander: probe it, set EXIO1 (LCD reset) and EXIO3
// (SD chip-select) as outputs initialized HIGH, leave others as
// inputs. Returns ESP_OK on success.
esp_err_t s3w_expander_init(void);

// Drive a single expander output pin high or low. The pin must
// already have been configured as an output during expander_init.
esp_err_t s3w_expander_set(uint8_t pin, bool level);

#ifdef __cplusplus
}
#endif
