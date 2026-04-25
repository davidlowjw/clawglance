if("${IDF_TARGET}" STREQUAL "esp32s3")
    idf_component_register(
        INCLUDE_DIRS
            "include"
        REQUIRES driver
        esp_common
        log
        esp_lcd
        fatfs
        sdmmc)
    # Vendor-locked precompiled BSP — only link for the original
    # ss559550@aliyun.com 3.5" board. The Waveshare S3 board has its
    # own open-source BSP in components/s3w_bsp/ that provides the
    # same `sys_int()` symbol.
    if(NOT CONFIG_CG_BOARD_S3_WAVESHARE)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/liblcd_bsp.a")
    endif()
else()
    # Non-S3 builds (e.g. CYD): header-only stub. Symbols (sys_int, etc.)
    # are provided by components/cyd_bsp instead. Headers stay here so
    # main.c's #include "bsp_board.h" continues to resolve.
    idf_component_register(
        INCLUDE_DIRS "include"
        REQUIRES driver)
endif()
