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
    target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/liblcd_bsp.a")
else()
    # Non-S3 builds (e.g. CYD): header-only stub. Symbols (sys_int, etc.)
    # are provided by components/cyd_bsp instead. Headers stay here so
    # main.c's #include "bsp_board.h" continues to resolve.
    idf_component_register(
        INCLUDE_DIRS "include"
        REQUIRES driver)
endif()
