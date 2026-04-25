# Waveshare ESP32-S3-Touch-LCD-3.5 BSP. Sources only compile when the
# matching Kconfig board choice is selected; the component itself is
# always registered with the full REQUIRES list so the IDF discovery
# pass — which runs before sdkconfig is populated — sees a stable
# dependency graph. (Gating idf_component_register itself on a CONFIG
# var locks in an empty deps list during discovery and silently drops
# the include paths in the build pass.)
if(CONFIG_CG_BOARD_S3_WAVESHARE)
    set(s3w_srcs
        "s3w_bsp.c"
        "s3w_lcd.c"
        "s3w_lvgl.c"
        "s3w_expander.c"
        "s3w_touch.c")
else()
    set(s3w_srcs)
endif()

idf_component_register(
    SRCS ${s3w_srcs}
    PRIV_INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_lcd
        esp_timer
        lvgl
        lcd_bsp
        lv_port
        main
)
