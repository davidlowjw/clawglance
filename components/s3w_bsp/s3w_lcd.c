// ST7796 driver for the Waveshare 3.5" board. Lifted from cyd_bsp/
// cyd_lcd.c (CG_BOARD_CYD35 branch) — both boards use ST7796 over SPI
// with identical init sequences. Differences: SPI host/pins and the
// fact that this board ties LCD CS to GND, so cs_gpio_num=-1.
#include "s3w_lcd.h"
#include "s3w_pins.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "s3w_lcd";

static esp_lcd_panel_io_handle_t s_io = NULL;
static s3w_lcd_flush_done_cb_t   s_done_cb = NULL;
static void                     *s_done_user = NULL;

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t flags; // low 5 bits = data length, 0x80 = delay 100ms, 0xFF = end
} lcd_cmd_t;

// ST7796 init — same as the CYD 3.5" panel.
static const lcd_cmd_t LCD_INIT[] = {
    {0x11, {0}, 0x80},                                 // Sleep out + 100ms
    {0x36, {0x28}, 1},                                 // MADCTL — landscape, MV+BGR
    {0x3A, {0x55}, 1},                                 // COLMOD — 16 bit/pixel
    {0xB7, {0x07}, 1},                                 // Entry mode
    {0xBB, {0x20}, 1},                                 // VCOMS
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
    {0x21, {0}, 0},                                    // Display inversion ON (Waveshare panel batch)
    {0x29, {0}, 0x80},                                 // Display on + 100ms
    {0x00, {0}, 0xFF},                                 // End of table
};

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

void s3w_lcd_set_done_cb(s3w_lcd_flush_done_cb_t cb, void *user_ctx) {
    s_done_user = user_ctx;
    s_done_cb   = cb;   // function pointer last so the ISR can't see a stale ctx
}

void s3w_lcd_init(void) {
    ESP_LOGI(TAG, "SPI bus %d: SCK=%d MOSI=%d MISO=%d  DC=%d  CS=tied-low",
             S3W_LCD_HOST, S3W_LCD_PIN_SCLK, S3W_LCD_PIN_MOSI,
             S3W_LCD_PIN_MISO, S3W_LCD_PIN_DC);

    spi_bus_config_t bus = {
        .sclk_io_num     = S3W_LCD_PIN_SCLK,
        .mosi_io_num     = S3W_LCD_PIN_MOSI,
        .miso_io_num     = S3W_LCD_PIN_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = S3W_LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(S3W_LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num         = S3W_LCD_PIN_DC,
        .cs_gpio_num         = S3W_LCD_PIN_CS,   // -1: CS tied low on board
        .pclk_hz             = S3W_LCD_PIXEL_CLOCK_HZ,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
        .lcd_cmd_bits        = S3W_LCD_CMD_BITS,
        .lcd_param_bits      = S3W_LCD_PARAM_BITS,
        .on_color_trans_done = on_color_done,
        .user_ctx            = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)S3W_LCD_HOST,
                                              &io_cfg, &s_io));

    ESP_LOGI(TAG, "Running ST7796 init sequence");
    for (int i = 0; LCD_INIT[i].flags != 0xFF; i++) {
        const lcd_cmd_t *c = &LCD_INIT[i];
        uint8_t len = c->flags & 0x1F;
        send_cmd(c->cmd, c->data, len);
        if (c->flags & 0x80) vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "ST7796 ready: %dx%d landscape", S3W_LCD_H_RES, S3W_LCD_V_RES);
}

void s3w_lcd_draw(int x0, int y0, int x1, int y1, const void *color_data) {
    set_window(x0, y0, x1, y1);
    size_t len = (size_t)(x1 - x0 + 1) * (y1 - y0 + 1) * sizeof(uint16_t);
    esp_lcd_panel_io_tx_color(s_io, 0x2C, color_data, len);
}

void s3w_lcd_fill_solid(uint16_t color) {
    const int rows = 40;
    const size_t pixels = (size_t)S3W_LCD_H_RES * rows;
    uint16_t *line = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line) {
        ESP_LOGE(TAG, "fill_solid: malloc failed");
        return;
    }
    uint16_t swapped = (color >> 8) | (color << 8);
    for (size_t i = 0; i < pixels; i++) line[i] = swapped;

    for (int y = 0; y < S3W_LCD_V_RES; y += rows) {
        int y_end = y + rows - 1;
        if (y_end >= S3W_LCD_V_RES) y_end = S3W_LCD_V_RES - 1;
        set_window(0, y, S3W_LCD_H_RES - 1, y_end);
        size_t len = (size_t)S3W_LCD_H_RES * (y_end - y + 1) * sizeof(uint16_t);
        esp_lcd_panel_io_tx_color(s_io, 0x2C, line, len);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    heap_caps_free(line);
}
