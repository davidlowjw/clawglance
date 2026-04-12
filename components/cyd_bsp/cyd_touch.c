// XPT2046 resistive touch driver for CYD boards with NVS-based
// per-device calibration. On first boot (or when cal data is missing),
// a 2-point crosshair calibration screen is shown before the UI loads.

#include "cyd_touch.h"
#include "cyd_pins.h"
#include "cyd_lcd.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "cyd_touch";

static spi_device_handle_t s_touch_dev = NULL;
static lv_indev_drv_t      s_indev_drv;

// XPT2046 command bytes (12-bit mode, differential reference)
#define XPT_CMD_X  0xD0
#define XPT_CMD_Y  0x90
#define XPT_CMD_Z1 0xB0
#define XPT_CMD_Z2 0xC0

#define TOUCH_Z_THRESHOLD 300

// ---- Calibration data ----
// Two calibration points at known display positions. The mapping
// function interpolates linearly between them, automatically handling
// axis swapping and inversion per board.
#define CAL_PCT 20  // crosshair placed at 20% / 80% of screen
#define CAL_A_DX  (CYD_LCD_H_RES * CAL_PCT / 100)
#define CAL_A_DY  (CYD_LCD_V_RES * CAL_PCT / 100)
#define CAL_B_DX  (CYD_LCD_H_RES * (100 - CAL_PCT) / 100)
#define CAL_B_DY  (CYD_LCD_V_RES * (100 - CAL_PCT) / 100)

typedef struct {
    int16_t a_raw_x, a_raw_y; // raw ADC at display point A
    int16_t b_raw_x, b_raw_y; // raw ADC at display point B
    bool valid;
} touch_cal_t;

static touch_cal_t s_cal = { .valid = false };

// Fallback calibration (used if NVS has no data AND calibration is skipped)
#define FALLBACK_CAL_X_MIN   200
#define FALLBACK_CAL_X_MAX  3900
#define FALLBACK_CAL_Y_MIN   300
#define FALLBACK_CAL_Y_MAX  3800

// ---- SPI read ----
static uint16_t xpt_read_channel(uint8_t cmd) {
    spi_transaction_t t = {
        .length    = 24,
        .tx_data   = { cmd, 0, 0 },
        .flags     = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    spi_device_polling_transmit(s_touch_dev, &t);
    uint16_t raw = ((uint16_t)t.rx_data[1] << 8) | t.rx_data[2];
    return raw >> 3;
}

// Read raw touch. Returns true if pressed with sufficient pressure.
static bool read_raw(int16_t *raw_x, int16_t *raw_y) {
#ifndef CG_BOARD_CYD35
    if (gpio_get_level(CYD_TOUCH_PIN_IRQ) != 0) return false;
#endif
    uint16_t z1 = xpt_read_channel(XPT_CMD_Z1);
    uint16_t z2 = xpt_read_channel(XPT_CMD_Z2);
    int z = z1 + (4095 - z2);
    if (z < TOUCH_Z_THRESHOLD) return false;

    uint32_t sx = 0, sy = 0;
    for (int i = 0; i < 4; i++) {
        sx += xpt_read_channel(XPT_CMD_X);
        sy += xpt_read_channel(XPT_CMD_Y);
    }
    *raw_x = (int16_t)(sx / 4);
    *raw_y = (int16_t)(sy / 4);
    return true;
}

// ---- NVS load/save ----
static bool nvs_load_cal(touch_cal_t *cal) {
    nvs_handle_t h;
    if (nvs_open("clawglance", NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = true;
    int16_t v;
    ok = ok && nvs_get_i16(h, "tcal_arx", &v) == ESP_OK; cal->a_raw_x = v;
    ok = ok && nvs_get_i16(h, "tcal_ary", &v) == ESP_OK; cal->a_raw_y = v;
    ok = ok && nvs_get_i16(h, "tcal_brx", &v) == ESP_OK; cal->b_raw_x = v;
    ok = ok && nvs_get_i16(h, "tcal_bry", &v) == ESP_OK; cal->b_raw_y = v;
    nvs_close(h);
    cal->valid = ok;
    if (ok) {
        ESP_LOGI(TAG, "Loaded cal from NVS: A(%d,%d) B(%d,%d)",
                 cal->a_raw_x, cal->a_raw_y, cal->b_raw_x, cal->b_raw_y);
    }
    return ok;
}

static void nvs_save_cal(const touch_cal_t *cal) {
    nvs_handle_t h;
    if (nvs_open("clawglance", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i16(h, "tcal_arx", cal->a_raw_x);
    nvs_set_i16(h, "tcal_ary", cal->a_raw_y);
    nvs_set_i16(h, "tcal_brx", cal->b_raw_x);
    nvs_set_i16(h, "tcal_bry", cal->b_raw_y);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Saved cal to NVS: A(%d,%d) B(%d,%d)",
             cal->a_raw_x, cal->a_raw_y, cal->b_raw_x, cal->b_raw_y);
}

// ---- LCD drawing helpers for calibration screen ----
static void draw_crosshair(int cx, int cy, uint16_t color) {
    // Draw a small + shape (11px arms). Use a 1-pixel-wide line buffer.
    const int arm = 8;
    uint16_t swapped = (color >> 8) | (color << 8); // byte-swap for SPI

    // Horizontal arm
    int x0 = cx - arm, x1 = cx + arm;
    if (x0 < 0) x0 = 0;
    if (x1 >= CYD_LCD_H_RES) x1 = CYD_LCD_H_RES - 1;
    int w = x1 - x0 + 1;
    uint16_t *buf = heap_caps_malloc(w * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf) {
        for (int i = 0; i < w; i++) buf[i] = swapped;
        cyd_lcd_draw(x0, cy, x1, cy, buf);
        vTaskDelay(pdMS_TO_TICKS(10)); // let DMA finish
        heap_caps_free(buf);
    }

    // Vertical arm
    int y0 = cy - arm, y1 = cy + arm;
    if (y0 < 0) y0 = 0;
    if (y1 >= CYD_LCD_V_RES) y1 = CYD_LCD_V_RES - 1;
    int h = y1 - y0 + 1;
    buf = heap_caps_malloc(h * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf) {
        for (int i = 0; i < h; i++) buf[i] = swapped;
        cyd_lcd_draw(cx, y0, cx, y1, buf);
        vTaskDelay(pdMS_TO_TICKS(10));
        heap_caps_free(buf);
    }
}

// Wait for a stable touch, return averaged raw values.
static void wait_for_tap(int16_t *rx, int16_t *ry) {
    // Wait until released first (in case already touching)
    while (1) {
        int16_t dummy_x, dummy_y;
        if (!read_raw(&dummy_x, &dummy_y)) break;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    // Wait for stable press
    int16_t last_x = 0, last_y = 0;
    int stable = 0;
    while (stable < 3) {
        int16_t tx, ty;
        if (read_raw(&tx, &ty)) {
            if (stable > 0 && abs(tx - last_x) < 80 && abs(ty - last_y) < 80) {
                stable++;
            } else {
                stable = 1;
            }
            last_x = tx;
            last_y = ty;
        } else {
            stable = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    *rx = last_x;
    *ry = last_y;

    // Wait for release
    while (1) {
        int16_t dummy_x, dummy_y;
        if (!read_raw(&dummy_x, &dummy_y)) break;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---- Public API ----

void cyd_touch_init(void) {
#ifdef CG_BOARD_CYD35
    const int touch_host = CYD_LCD_HOST;
    ESP_LOGI(TAG, "Adding touch device on shared SPI host %d (CS=%d)",
             touch_host, CYD_TOUCH_PIN_CS);
#else
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
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = CYD_TOUCH_PIN_CS,
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(touch_host, &dev, &s_touch_dev));

    gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << CYD_TOUCH_PIN_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&irq_cfg);

    // Load calibration from NVS
    nvs_load_cal(&s_cal);

    ESP_LOGI(TAG, "XPT2046 ready (CS=%d, IRQ=%d, cal=%s)",
             CYD_TOUCH_PIN_CS, CYD_TOUCH_PIN_IRQ,
             s_cal.valid ? "loaded" : "none");
}

bool cyd_touch_has_calibration(void) {
    return s_cal.valid;
}

void cyd_touch_calibrate(void) {
    ESP_LOGI(TAG, "Starting touch calibration");

    // Point A: top-left area
    cyd_lcd_fill_solid(0x0000); // black
    draw_crosshair(CAL_A_DX, CAL_A_DY, 0xFFFF); // white
    ESP_LOGI(TAG, "Tap the top-left crosshair");
    wait_for_tap(&s_cal.a_raw_x, &s_cal.a_raw_y);
    ESP_LOGI(TAG, "Point A: raw(%d, %d) → display(%d, %d)",
             s_cal.a_raw_x, s_cal.a_raw_y, CAL_A_DX, CAL_A_DY);

    // Brief flash feedback
    cyd_lcd_fill_solid(0x07E0); // green flash
    vTaskDelay(pdMS_TO_TICKS(300));

    // Point B: bottom-right area
    cyd_lcd_fill_solid(0x0000);
    draw_crosshair(CAL_B_DX, CAL_B_DY, 0xFFFF);
    ESP_LOGI(TAG, "Tap the bottom-right crosshair");
    wait_for_tap(&s_cal.b_raw_x, &s_cal.b_raw_y);
    ESP_LOGI(TAG, "Point B: raw(%d, %d) → display(%d, %d)",
             s_cal.b_raw_x, s_cal.b_raw_y, CAL_B_DX, CAL_B_DY);

    // Save
    s_cal.valid = true;
    nvs_save_cal(&s_cal);

    // Done flash
    cyd_lcd_fill_solid(0x07E0);
    vTaskDelay(pdMS_TO_TICKS(300));
    cyd_lcd_fill_solid(0x0000);

    ESP_LOGI(TAG, "Calibration complete");
}

bool cyd_touch_read(int16_t *x, int16_t *y) {
    int16_t raw_x, raw_y;
    if (!read_raw(&raw_x, &raw_y)) return false;

    int dx, dy;

    if (s_cal.valid) {
        // General linear mapping from raw → display using calibration data.
        // raw_y maps to display X, raw_x maps to display Y (touch panel
        // is natively portrait, display runs landscape). Axis inversion
        // is handled automatically by the sign of the calibration deltas.
        int16_t a_ry = s_cal.a_raw_y, b_ry = s_cal.b_raw_y;
        int16_t a_rx = s_cal.a_raw_x, b_rx = s_cal.b_raw_x;

        if (b_ry != a_ry)
            dx = CAL_A_DX + (int)(raw_y - a_ry) * (CAL_B_DX - CAL_A_DX) / (b_ry - a_ry);
        else
            dx = CYD_LCD_H_RES / 2;

        if (b_rx != a_rx)
            dy = CAL_A_DY + (int)(raw_x - a_rx) * (CAL_B_DY - CAL_A_DY) / (b_rx - a_rx);
        else
            dy = CYD_LCD_V_RES / 2;
    } else {
        // Fallback: hardcoded constants (pre-calibration defaults)
        dx = (int)(raw_y - FALLBACK_CAL_Y_MIN) * CYD_LCD_H_RES / (FALLBACK_CAL_Y_MAX - FALLBACK_CAL_Y_MIN);
        dy = (int)(raw_x - FALLBACK_CAL_X_MIN) * CYD_LCD_V_RES / (FALLBACK_CAL_X_MAX - FALLBACK_CAL_X_MIN);
    }

    // Clamp
    if (dx < 0) dx = 0;
    if (dx >= CYD_LCD_H_RES) dx = CYD_LCD_H_RES - 1;
    if (dy < 0) dy = 0;
    if (dy >= CYD_LCD_V_RES) dy = CYD_LCD_V_RES - 1;

    *x = (int16_t)dx;
    *y = (int16_t)dy;
    return true;
}

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
