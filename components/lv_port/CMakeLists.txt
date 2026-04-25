if("${IDF_TARGET}" STREQUAL "esp32s3")
    idf_component_register(
        INCLUDE_DIRS
        "include"
        PRIV_REQUIRES
        driver
        lcd_bsp
        esp_lcd
        main
    )
    # Skip the precompiled lv_port for the Waveshare S3 board — symbols
    # come from components/s3w_bsp instead.
    if(NOT CONFIG_CG_BOARD_S3_WAVESHARE)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/liblv_port.a")
    endif()
else()
    # Non-S3 builds (e.g. CYD): header-only stub. lvgl_init / lv_port_sem_*
    # symbols are provided by components/cyd_bsp instead.
    idf_component_register(
        INCLUDE_DIRS "include"
        PRIV_REQUIRES driver esp_lcd
    )
endif()
