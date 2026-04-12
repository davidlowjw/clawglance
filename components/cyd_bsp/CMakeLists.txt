if("${IDF_TARGET}" STREQUAL "esp32")
    # Real CYD build: ILI9341 + XPT2046 drivers + LVGL wiring
    idf_component_register(
        SRCS
            "cyd_bsp.c"
            "cyd_lcd.c"
            "cyd_lvgl.c"
            "cyd_touch.c"
        PRIV_INCLUDE_DIRS "."
        REQUIRES
            driver
            esp_lcd
            esp_timer
            lvgl
            lcd_bsp
            lv_port
    )
else()
    # On non-CYD targets (esp32s3) the symbols come from the pre-compiled
    # lcd_bsp / lv_port .a files instead — this component is an empty stub
    # so main's REQUIRES line still resolves.
    idf_component_register()
endif()
