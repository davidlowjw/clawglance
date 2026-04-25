#include "s3w_expander.h"
#include "s3w_pins.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "s3w_exp";

// TCA9554 register map.
#define REG_INPUT   0x00
#define REG_OUTPUT  0x01
#define REG_POL     0x02
#define REG_CONFIG  0x03   // bit n: 0 = output, 1 = input. Default 0xFF.

static bool s_i2c_inited = false;
static uint8_t s_output_state = 0xFF;   // mirror of REG_OUTPUT

esp_err_t s3w_i2c_init(void) {
    if (s_i2c_inited) return ESP_OK;
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = S3W_I2C_PIN_SDA,
        .scl_io_num = S3W_I2C_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = S3W_I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(S3W_I2C_PORT, &cfg);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(S3W_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) return err;
    s_i2c_inited = true;
    ESP_LOGI(TAG, "I2C%d up: SDA=%d SCL=%d @ %d Hz",
             S3W_I2C_PORT, S3W_I2C_PIN_SDA, S3W_I2C_PIN_SCL, S3W_I2C_FREQ_HZ);
    return ESP_OK;
}

static esp_err_t reg_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(S3W_I2C_PORT, S3W_TCA9554_ADDR,
                                      buf, sizeof(buf), pdMS_TO_TICKS(50));
}

esp_err_t s3w_expander_init(void) {
    esp_err_t err = s3w_i2c_init();
    if (err != ESP_OK) return err;

    // Pre-set output latches HIGH so the to-be-outputs don't glitch low
    // when we flip them to outputs.
    s_output_state = 0xFF;
    err = reg_write(REG_OUTPUT, s_output_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TCA9554 not responding at 0x%02X (err %d)", S3W_TCA9554_ADDR, err);
        return err;
    }

    // Configure: 0 = output, 1 = input. EXIO1 + EXIO3 are outputs.
    uint8_t config = 0xFF;
    config &= ~(1u << S3W_EXIO_LCD_RST);
    config &= ~(1u << S3W_EXIO_SD_CS);
    err = reg_write(REG_CONFIG, config);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "TCA9554 ready @ 0x%02X (EXIO%d=LCD_RST, EXIO%d=SD_CS as outputs)",
             S3W_TCA9554_ADDR, S3W_EXIO_LCD_RST, S3W_EXIO_SD_CS);
    return ESP_OK;
}

esp_err_t s3w_expander_set(uint8_t pin, bool level) {
    if (level) s_output_state |=  (1u << pin);
    else       s_output_state &= ~(1u << pin);
    return reg_write(REG_OUTPUT, s_output_state);
}
