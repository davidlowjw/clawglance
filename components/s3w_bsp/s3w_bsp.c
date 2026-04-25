// Waveshare ESP32-S3-Touch-LCD-3.5 board init — replaces the
// vendor-locked precompiled lcd_bsp `sys_int()` for this board. Brings
// up the I2C bus + TCA9554 expander, pulses the LCD reset line, and
// turns on the backlight. Display init proper happens in s3w_lcd_init()
// from inside lvgl_init().
#include "bsp_board.h"
#include "s3w_pins.h"
#include "s3w_expander.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "s3w_bsp";

static void backlight_on(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << S3W_LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(S3W_LCD_PIN_BL, S3W_LCD_BL_ON_LEVEL);
}

static void pulse_panel_reset(void) {
    // EXIO1 drives both LCD RST and touch RST (active low).
    s3w_expander_set(S3W_EXIO_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    s3w_expander_set(S3W_EXIO_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void sys_int(void) {
    ESP_LOGI(TAG, "Waveshare ESP32-S3-Touch-LCD-3.5 board init");

    // 1. I2C + expander first — needed to release the LCD reset line.
    if (s3w_expander_init() != ESP_OK) {
        ESP_LOGE(TAG, "Expander init failed — display will not work");
        return;
    }

    // 2. Pulse LCD/touch reset via the expander.
    pulse_panel_reset();

    // 3. Backlight ON. Doing this before the panel is initialised will
    //    just show RAM contents (usually noise/black) for a few hundred
    //    ms — acceptable. Doing it later means a noticeable dark flash
    //    after the splash screen.
    backlight_on();
    ESP_LOGI(TAG, "Backlight ON (GPIO%d)", S3W_LCD_PIN_BL);
}
