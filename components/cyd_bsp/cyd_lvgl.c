// LVGL bring-up for the CYD board. Wires LVGL's flush callback to the
// ILI9341 SPI driver in cyd_lcd.c.
#include "lvgl.h"
#include "lvgl_init.h"
#include "lv_port_disp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "cyd_pins.h"
#include "cyd_lcd.h"
#include "cyd_touch.h"

static const char *TAG = "cyd_lvgl";

#define LVGL_BUF_LINES   40

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;
static lv_color_t        *s_buf1;
static lv_color_t        *s_buf2;

static SemaphoreHandle_t  s_lvgl_mutex;

void lv_port_sem_take(void) {
    xSemaphoreTakeRecursive(s_lvgl_mutex, portMAX_DELAY);
}

void lv_port_sem_give(void) {
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}

// Called from the SPI ISR after the previous tx_color batch has
// finished — tells LVGL the buffer can be reused.
static void on_lcd_done(void *user_ctx) {
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(drv);
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    cyd_lcd_draw(area->x1, area->y1, area->x2, area->y2, color_p);
}

static void lvgl_tick_cb(void *arg) {
    lv_tick_inc(1);
}

static void lvgl_task(void *arg) {
    while (1) {
        lv_port_sem_take();
        uint32_t delay = lv_timer_handler();
        lv_port_sem_give();
        if (delay > 100) delay = 100;
        if (delay < 5)   delay = 5;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

void lvgl_init(void) {
    ESP_LOGI(TAG, "LVGL init (CYD)");

    // 1. Bring up the panel BEFORE LVGL so we can do a sanity-check fill.
    //    Done callback is registered LATER, after the LVGL display driver
    //    is fully initialized — otherwise the ISR fires into a NULL drv.
    cyd_lcd_init();

    // 2. Now LVGL.
    lv_init();

    const size_t buf_pixels = CYD_LCD_H_RES * LVGL_BUF_LINES;
    s_buf1 = heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    s_buf2 = heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(s_buf1 && s_buf2);
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = CYD_LCD_H_RES;
    s_disp_drv.ver_res  = CYD_LCD_V_RES;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    // Now safe to enable the SPI completion ISR — drv is fully populated.
    cyd_lcd_set_done_cb(on_lcd_done, &s_disp_drv);

    // 3. Touch input.
    cyd_touch_init();
    cyd_touch_lvgl_register();

    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    assert(s_lvgl_mutex);

    const esp_timer_create_args_t tick_args = {
        .callback = &lvgl_tick_cb,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 1000); // 1 ms

    xTaskCreate(lvgl_task, "lvgl", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "LVGL ready: %dx%d, free internal heap: %lu",
             CYD_LCD_H_RES, CYD_LCD_V_RES,
             (unsigned long)esp_get_free_internal_heap_size());
}
