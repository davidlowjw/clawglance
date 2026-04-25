# Header-only "interface" component. Provides bsp_board.h (just the
# `sys_int()` declaration); the actual implementation lives in each
# board's BSP component (cyd_bsp/, s3w_bsp/, …).
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES driver
)
