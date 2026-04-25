#include "s3w_touch.h"
#include "s3w_pins.h"

#include "lvgl.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "s3w_touch";

static lv_indev_drv_t s_indev_drv;
static int16_t s_last_x = 0;
static int16_t s_last_y = 0;
static bool    s_last_pressed = false;

// FT6336 register map (selected):
//   0x02  TD_STATUS    — number of touch points (low nibble)
//   0x03  P1_XH        — bits[7:6] event flag, bits[3:0] X high nibble
//   0x04  P1_XL        — X low byte
//   0x05  P1_YH        — bits[7:4] touch ID, bits[3:0] Y high nibble
//   0x06  P1_YL        — Y low byte
#define FT6336_REG_TD_STATUS  0x02

void s3w_touch_init(void) {
    // I2C is already up and the panel reset has been pulsed (s3w_bsp.c
    // does both before this runs). Probe the vendor-ID register so a
    // dead bus produces a clear error log instead of silent no-touches.
    uint8_t reg = 0xA8, val;
    if (i2c_master_write_read_device(S3W_I2C_PORT, S3W_TOUCH_ADDR,
                                     &reg, 1, &val, 1, pdMS_TO_TICKS(20)) != ESP_OK) {
        ESP_LOGE(TAG, "FT6336 not responding at 0x%02X", S3W_TOUCH_ADDR);
        return;
    }

    // Disable auto-monitor: by default the chip drops scan rate to ~5 Hz
    // when idle, which can swallow short taps before LVGL's polling
    // catches them. Setting G_MONITOR (0x86) = 0 keeps it always-active.
    uint8_t buf[2] = { 0x86, 0x00 };
    i2c_master_write_to_device(S3W_I2C_PORT, S3W_TOUCH_ADDR,
                               buf, sizeof(buf), pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "FT6336 ready @ 0x%02X", S3W_TOUCH_ADDR);
}

// Returns true if at least one finger is currently down. Coordinates
// are written to *x, *y in the FT6336's native frame (matches the
// Waveshare panel: x in [0,479], y in [0,319] for landscape).
static bool read_touch(int16_t *x, int16_t *y) {
    uint8_t reg = FT6336_REG_TD_STATUS;
    uint8_t buf[5];
    esp_err_t err = i2c_master_write_read_device(S3W_I2C_PORT, S3W_TOUCH_ADDR,
                                                  &reg, 1, buf, sizeof(buf),
                                                  pdMS_TO_TICKS(20));
    if (err != ESP_OK) return false;
    uint8_t num = buf[0] & 0x0F;
    if (num == 0 || num > 2) return false;
    // FT6336 reports in the panel's native portrait frame (320×480),
    // but the display is in landscape (480×320, MADCTL 0x28 = MV+BGR).
    // Rotate 90° CW: landscape X = native Y, landscape Y = flip(native X).
    int16_t nx = ((buf[1] & 0x0F) << 8) | buf[2];
    int16_t ny = ((buf[3] & 0x0F) << 8) | buf[4];
    *x = ny;
    *y = (S3W_LCD_V_RES - 1) - nx;
    return true;
}

static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    int16_t x, y;
    if (read_touch(&x, &y)) {
        s_last_x = x;
        s_last_y = y;
        s_last_pressed = true;
    } else {
        s_last_pressed = false;
    }
    data->point.x = s_last_x;
    data->point.y = s_last_y;
    data->state   = s_last_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

void s3w_touch_lvgl_register(void) {
    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);
    ESP_LOGI(TAG, "Touch indev registered");
}
