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
