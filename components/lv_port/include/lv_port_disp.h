/**
 * @file lv_port_disp_templ.h
 *
 */

 /*Copy this file as "lv_port_disp.h" and set this value to "1" to enable content*/
#if 1

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_lcd_panel_io.h"
#include "soc/soc_caps.h"
#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"


#include "esp_timer.h"


/**
 * @brief Initialize related work for lvgl.
 *
 */
esp_lcd_panel_handle_t  *lv_port_disp_init(void);

/**
 * @brief Initialize lvgl tick task.
 *
 */
esp_err_t lv_port_tick_init(void);

/**
 * @brief Take the semaphore.
 * @note  It should be called before manipulate lvgl gui.
 *
 */
void lv_port_sem_take(void);

/**
 * @brief Give the semaphore.
 * @note  It should be called after manipulate lvgl gui.
 *
 */
void lv_port_sem_give(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_DISP_TEMPL_H*/

#endif /*Disable/Enable content*/
