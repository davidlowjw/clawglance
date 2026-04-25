# Header-only LVGL port interface (lvgl_init.h, lv_port_disp.h,
# lv_port_indev.h, lv_port_fs.h). Each board's BSP component provides
# the symbols (lvgl_init, lv_port_sem_take/give, …).
idf_component_register(
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver esp_lcd
)
