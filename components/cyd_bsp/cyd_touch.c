// XPT2046 resistive touch driver for the CYD.
//
// The XPT2046 sits on its own SPI bus (SPI3_HOST / VSPI) separate from
// the ILI9341 display. We bit-bang the SPI reads because the XPT2046's
// protocol (24-bit command+response) doesn't map cleanly to esp-idf's
// spi_device_transmit, and the touch polling rate is low enough that
// bit-bang is fine.
//
// Actually, we DO use the SPI master driver — the XPT2046 works fine
// with standard 8-bit SPI transactions when you handle the 12-bit
// response parsing yourself.

#include "cyd_touch.h"
#include "cyd_pins.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "cyd_touch";

static spi_device_handle_t s_touch_dev = NULL;
static lv_indev_drv_t      s_indev_drv;

// XPT2046 command bytes (start=1, 12-bit mode, differential reference)
#define XPT_CMD_X  0xD0  // channel 5 (X), 12-bit, differential
#define XPT_CMD_Y  0x90  // channel 1 (Y), 12-bit, differential
#define XPT_CMD_Z1 0xB0  // Z1 pressure
#define XPT_CMD_Z2 0xC0  // Z2 pressure

// Calibration constants — map raw ADC (0..4095) to display pixels.
// These are typical for the classic CYD in landscape (MADCTL=0x28).
// Adjust after running the touch coordinate test.
#define CAL_X_MIN   200
#define CAL_X_MAX  3900
#define CAL_Y_MIN   300
#define CAL_Y_MAX  3800

// Pressure threshold — readings below this are "not touched"
#define TOUCH_Z_THRESHOLD 400

static uint16_t xpt_read_channel(uint8_t cmd) {
    spi_transaction_t t = {
        .length    = 24,   // 8-bit cmd + 16-bit response
        .tx_data   = { cmd, 0, 0 },
        .flags     = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    spi_device_polling_transmit(s_touch_dev, &t);
    // Response is in bits 1..12 of the 16-bit response (MSB first).
    uint16_t raw = ((uint16_t)t.rx_data[1] << 8) | t.rx_data[2];
    return raw >> 3; // 12-bit result
}

void cyd_touch_init(void) {
#ifdef CG_BOARD_CYD35
    // CYD 3.5": touch shares the display SPI bus (SPI2_HOST).
    // The bus is already initialized by cyd_lcd_init(); just add a device.
    const int touch_host = CYD_LCD_HOST;
    ESP_LOGI(TAG, "Adding touch device on shared SPI host %d (CS=%d)",
             touch_host, CYD_TOUCH_PIN_CS);
#else
    // CYD 2.8": touch has its own SPI bus (SPI3_HOST / VSPI).
    const int touch_host = CYD_TOUCH_HOST;
    ESP_LOGI(TAG, "Init SPI bus on host %d (SCK=%d MOSI=%d MISO=%d)",
             touch_host, CYD_TOUCH_PIN_SCLK, CYD_TOUCH_PIN_MOSI, CYD_TOUCH_PIN_MISO);

    spi_bus_config_t bus = {
        .sclk_io_num     = CYD_TOUCH_PIN_SCLK,
        .mosi_io_num     = CYD_TOUCH_PIN_MOSI,
        .miso_io_num     = CYD_TOUCH_PIN_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 32,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(touch_host, &bus, SPI_DMA_DISABLED));
#endif

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 2 * 1000 * 1000, // 2 MHz — XPT2046 is slow
        .mode           = 0,
        .spics_io_num   = CYD_TOUCH_PIN_CS,
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(touch_host, &dev, &s_touch_dev));

    // Configure IRQ pin as input (optional — we poll instead)
    gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << CYD_TOUCH_PIN_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&irq_cfg);

    ESP_LOGI(TAG, "XPT2046 ready (CS=%d, IRQ=%d)", CYD_TOUCH_PIN_CS, CYD_TOUCH_PIN_IRQ);
}

bool cyd_touch_read(int16_t *x, int16_t *y) {
#ifndef CG_BOARD_CYD35
    // CYD 2.8": IRQ pin is wired — quick check before SPI read
    if (gpio_get_level(CYD_TOUCH_PIN_IRQ) != 0) return false;
#endif

    // Read pressure to confirm real touch
    uint16_t z1 = xpt_read_channel(XPT_CMD_Z1);
    uint16_t z2 = xpt_read_channel(XPT_CMD_Z2);
    int z = z1 + (4095 - z2);
    if (z < TOUCH_Z_THRESHOLD) return false;

    // Average multiple samples for stability
    uint32_t raw_x = 0, raw_y = 0;
    const int samples = 4;
    for (int i = 0; i < samples; i++) {
        raw_x += xpt_read_channel(XPT_CMD_X);
        raw_y += xpt_read_channel(XPT_CMD_Y);
    }
    raw_x /= samples;
    raw_y /= samples;

    // Map raw ADC to display coordinates (landscape).
    // XPT2046 X channel maps to display Y, and vice versa, because
    // the touch panel is natively portrait while we run landscape.
#ifdef CG_BOARD_CYD35
    // CYD 3.5": X axis is inverted relative to 2.8"
    int dx = (int)(CAL_Y_MAX - raw_y) * CYD_LCD_H_RES / (CAL_Y_MAX - CAL_Y_MIN);
#else
    int dx = (int)(raw_y - CAL_Y_MIN) * CYD_LCD_H_RES / (CAL_Y_MAX - CAL_Y_MIN);
#endif
    int dy = (int)(raw_x - CAL_X_MIN) * CYD_LCD_V_RES / (CAL_X_MAX - CAL_X_MIN);

    // Clamp
    if (dx < 0) dx = 0;
    if (dx >= CYD_LCD_H_RES) dx = CYD_LCD_H_RES - 1;
    if (dy < 0) dy = 0;
    if (dy >= CYD_LCD_V_RES) dy = CYD_LCD_V_RES - 1;

    *x = (int16_t)dx;
    *y = (int16_t)dy;
    return true;
}

// LVGL input device read callback
static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    int16_t tx, ty;
    if (cyd_touch_read(&tx, &ty)) {
        data->point.x = tx;
        data->point.y = ty;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void cyd_touch_lvgl_register(void) {
    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);
    ESP_LOGI(TAG, "LVGL touch input registered");
}
