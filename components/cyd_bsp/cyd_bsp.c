// CYD board init — replaces the precompiled lcd_bsp `sys_int()` for the
// classic ESP32-2432S028R. Currently only powers on the backlight; the
// real ILI9341 panel init is wired up in cyd_lvgl.c during lvgl_init().
#include "bsp_board.h"
#include "cyd_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "cyd_bsp";

void sys_int(void) {
#ifdef CG_BOARD_CYD35
    ESP_LOGI(TAG, "CYD 3.5\" (ESP32-2432S035) board init");
#else
    ESP_LOGI(TAG, "CYD 2.8\" (ESP32-2432S028R) board init");
#endif

    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << CYD_LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(CYD_LCD_PIN_BL, CYD_LCD_BL_ON_LEVEL);
    ESP_LOGI(TAG, "Backlight ON (GPIO%d)", CYD_LCD_PIN_BL);
}
