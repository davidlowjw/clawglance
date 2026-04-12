// ILI9341 driver for the CYD (ESP32-2432S028R), landscape 320×240.
//
// Uses esp_lcd_panel_io_spi for the SPI/DC plumbing (with DMA + done
// callback) but rolls its own ILI9341 init sequence and CASET/RASET/
// RAMWR write path. We deliberately don't depend on
// `esp_lcd_new_panel_ili9341` because that lives in the IDF component
// registry, not in the IDF core — keeping it self-contained means zero
// extra component pulls.

#include "cyd_lcd.h"
#include "cyd_pins.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "cyd_lcd";

static esp_lcd_panel_io_handle_t s_io = NULL;
static cyd_lcd_flush_done_cb_t   s_done_cb = NULL;
static void                     *s_done_user = NULL;

// ILI9341 init descriptor: cmd byte, param bytes, then a flag byte
// where bit 7 means "delay <param0> ms after the command", and 0xFF
// terminates the table.
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t flags; // low 5 bits = data length, 0x80 = delay, 0xFF = end
} lcd_cmd_t;

#if defined(CG_BOARD_CYD_ST7789)
// ---- ST7789 init (ESP32-2432S028R variant, 320x240) ----
static const lcd_cmd_t LCD_INIT[] = {
    {0x11, {0}, 0x80},                                 // Sleep out + 100ms
    {0x36, {0x60}, 1},                                 // MADCTL — landscape, MX+MV (RGB order)
    {0x3A, {0x55}, 1},                                 // COLMOD — 16 bit/pixel
    {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},         // Porch setting
    {0xB7, {0x35}, 1},                                 // Gate control
    {0xBB, {0x19}, 1},                                 // VCOMS setting
    {0xC0, {0x2C}, 1},                                 // LCM control
    {0xC2, {0x01}, 1},                                 // VDV/VRH enable
    {0xC3, {0x12}, 1},                                 // VRH set (4.45V)
    {0xC4, {0x20}, 1},                                 // VDV set
    {0xC6, {0x0F}, 1},                                 // Frame rate (60Hz)
    {0xD0, {0xA4, 0xA1}, 2},                           // Power control
    {0xE0, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
            0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}, 14}, // Positive gamma
    {0xE1, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
            0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}, 14}, // Negative gamma
    {0x20, {0}, 0},                                    // Display inversion off
    {0x29, {0}, 0x80},                                 // Display on + 100ms
    {0x00, {0}, 0xFF},                                 // End of table
};
#elif defined(CG_BOARD_CYD35)
// ---- ST7796U init (ESP32-2432S035, 480x320) ----
static const lcd_cmd_t LCD_INIT[] = {
    {0x11, {0}, 0x80},                                 // Sleep out + 100ms
    {0x36, {0x28}, 1},                                 // MADCTL — landscape, BGR
    {0x3A, {0x55}, 1},                                 // COLMOD — 16 bit/pixel
    {0xB7, {0x07}, 1},                                 // Entry mode
    {0xBB, {0x20}, 1},                                 // VCOMS setting
    {0xC0, {0x2C}, 1},                                 // LCM control
    {0xC2, {0x01}, 1},                                 // VDV/VRH enable
    {0xC3, {0x13}, 1},                                 // VRH set (4.5V)
    {0xC4, {0x20}, 1},                                 // VDV set
    {0xC6, {0x0F}, 1},                                 // Frame rate (60Hz)
    {0xD0, {0xA4, 0xA1}, 2},                           // Power control
    {0xE0, {0xF0, 0x04, 0x08, 0x09, 0x08, 0x15, 0x2F,
            0x42, 0x46, 0x28, 0x15, 0x16, 0x29, 0x2D}, 14}, // Positive gamma
    {0xE1, {0xF0, 0x08, 0x0C, 0x0B, 0x0B, 0x14, 0x28,
            0x33, 0x44, 0x27, 0x13, 0x12, 0x27, 0x2D}, 14}, // Negative gamma
    {0x20, {0}, 0},                                    // Display inversion off
    {0x29, {0}, 0x80},                                 // Display on + 100ms
    {0x00, {0}, 0xFF},                                 // End of table
};
#else
// ---- ILI9341 init (ESP32-2432S028R, 320x240) ----
static const lcd_cmd_t LCD_INIT[] = {
    {0xCF, {0x00, 0x83, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},                                 // Power control 1
    {0xC1, {0x11}, 1},                                 // Power control 2
    {0xC5, {0x35, 0x3E}, 2},                           // VCOM control 1
    {0xC7, {0xBE}, 1},                                 // VCOM control 2
    {0x36, {0x28}, 1},                                 // MADCTL — landscape, BGR
    {0x3A, {0x55}, 1},                                 // COLMOD — 16 bit/pixel
    {0xB1, {0x00, 0x1B}, 2},                           // Frame rate
    {0xF2, {0x08}, 1},                                 // 3Gamma off
    {0x26, {0x01}, 1},                                 // Gamma curve select
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45,
            0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05,
            0x00}, 15},                                // Positive gamma
    {0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A,
            0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A,
            0x1F}, 15},                                // Negative gamma
    {0xB7, {0x07}, 1},                                 // Entry mode
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},               // Display function ctrl
    {0x11, {0}, 0x80},                                 // Sleep out + 100ms
    {0x29, {0}, 0x80},                                 // Display on  + 100ms
    {0x00, {0}, 0xFF},                                 // End of table
};
#endif

static bool on_color_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *data,
                          void *user_ctx) {
    if (s_done_cb) s_done_cb(s_done_user);
    return false;
}

static void send_cmd(uint8_t cmd, const uint8_t *param, size_t len) {
    esp_lcd_panel_io_tx_param(s_io, cmd, len ? param : NULL, len);
}

static void set_window(int x0, int y0, int x1, int y1) {
    uint8_t buf[4];
    buf[0] = (x0 >> 8) & 0xFF; buf[1] = x0 & 0xFF;
    buf[2] = (x1 >> 8) & 0xFF; buf[3] = x1 & 0xFF;
    send_cmd(0x2A, buf, 4); // CASET
    buf[0] = (y0 >> 8) & 0xFF; buf[1] = y0 & 0xFF;
    buf[2] = (y1 >> 8) & 0xFF; buf[3] = y1 & 0xFF;
    send_cmd(0x2B, buf, 4); // RASET
}

void cyd_lcd_set_done_cb(cyd_lcd_flush_done_cb_t done_cb, void *user_ctx) {
    s_done_user = user_ctx;
    // Set the function pointer last so the ISR sees a fully-populated user ctx.
    s_done_cb   = done_cb;
}

void cyd_lcd_init(void) {
    ESP_LOGI(TAG, "Init SPI bus on host %d (SCK=%d MOSI=%d MISO=%d)",
             CYD_LCD_HOST, CYD_LCD_PIN_SCLK, CYD_LCD_PIN_MOSI, CYD_LCD_PIN_MISO);

    spi_bus_config_t bus = {
        .sclk_io_num     = CYD_LCD_PIN_SCLK,
        .mosi_io_num     = CYD_LCD_PIN_MOSI,
        .miso_io_num     = CYD_LCD_PIN_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = CYD_LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CYD_LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num         = CYD_LCD_PIN_DC,
        .cs_gpio_num         = CYD_LCD_PIN_CS,
        .pclk_hz             = CYD_LCD_PIXEL_CLOCK_HZ,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
        .lcd_cmd_bits        = CYD_LCD_CMD_BITS,
        .lcd_param_bits      = CYD_LCD_PARAM_BITS,
        .on_color_trans_done = on_color_done,
        .user_ctx            = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)CYD_LCD_HOST,
                                              &io_cfg, &s_io));

#if defined(CG_BOARD_CYD_ST7789)
    ESP_LOGI(TAG, "Running ST7789 init sequence");
#elif defined(CG_BOARD_CYD35)
    ESP_LOGI(TAG, "Running ST7796U init sequence");
#else
    ESP_LOGI(TAG, "Running ILI9341 init sequence");
#endif
    for (int i = 0; LCD_INIT[i].flags != 0xFF; i++) {
        const lcd_cmd_t *c = &LCD_INIT[i];
        uint8_t len = c->flags & 0x1F;
        send_cmd(c->cmd, c->data, len);
        if (c->flags & 0x80) vTaskDelay(pdMS_TO_TICKS(100));
    }

#if defined(CG_BOARD_CYD_ST7789)
    ESP_LOGI(TAG, "ST7789 ready: %dx%d landscape", CYD_LCD_H_RES, CYD_LCD_V_RES);
#elif defined(CG_BOARD_CYD35)
    ESP_LOGI(TAG, "ST7796U ready: %dx%d landscape", CYD_LCD_H_RES, CYD_LCD_V_RES);
#else
    ESP_LOGI(TAG, "ILI9341 ready: %dx%d landscape", CYD_LCD_H_RES, CYD_LCD_V_RES);
#endif
}

void cyd_lcd_draw(int x0, int y0, int x1, int y1, const void *color_data) {
    set_window(x0, y0, x1, y1);
    size_t len = (size_t)(x1 - x0 + 1) * (y1 - y0 + 1) * sizeof(uint16_t);
    esp_lcd_panel_io_tx_color(s_io, 0x2C, color_data, len);
}

void cyd_lcd_fill_solid(uint16_t color) {
    // Fill in 40-line stripes — bounded by the bus max_transfer_sz.
    const int rows = 40;
    const size_t pixels = (size_t)CYD_LCD_H_RES * rows;
    uint16_t *line = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line) {
        ESP_LOGE(TAG, "fill_solid: malloc failed");
        return;
    }
    // RGB565 is little-endian on the wire when DMA writes 16-bit words,
    // but the panel expects MSB first → byte-swap.
    uint16_t swapped = (color >> 8) | (color << 8);
    for (size_t i = 0; i < pixels; i++) line[i] = swapped;

    for (int y = 0; y < CYD_LCD_V_RES; y += rows) {
        int y_end = y + rows - 1;
        if (y_end >= CYD_LCD_V_RES) y_end = CYD_LCD_V_RES - 1;
        set_window(0, y, CYD_LCD_H_RES - 1, y_end);
        size_t len = (size_t)CYD_LCD_H_RES * (y_end - y + 1) * sizeof(uint16_t);
        // Use tx_color so the DMA path matches the LVGL flush path.
        esp_lcd_panel_io_tx_color(s_io, 0x2C, line, len);
    }
    // Wait for the queued transactions to drain. The IO layer doesn't
    // expose a sync API, so we just sleep — fill_solid is a one-shot
    // bring-up helper, not a hot path.
    vTaskDelay(pdMS_TO_TICKS(50));
    heap_caps_free(line);
}
