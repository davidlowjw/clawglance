# ClawGlance PC simulator — headless LVGL render to PNG.
#
# Usage:
#   make            # build and run, drops PNGs in sim/out/
#   make build      # build only
#   make clean

LVGL_DIR  := ../components/lvgl/lvgl
MAIN_DIR  := ../main

# LVGL core sources — exclude the SDL draw backend (we use the default
# software renderer), tests, examples, and demos.
LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name "*.c" \
	-not -path "*/draw/sdl/*" \
	-not -path "*/draw/nxp/*" \
	-not -path "*/draw/stm32_dma2d/*" \
	-not -path "*/draw/arm2d/*" \
	-not -path "*/draw/renesas/*" \
	-not -path "*/draw/swm341_dma2d/*" \
	-not -path "*/tests/*" \
	-not -path "*/examples/*" \
	-not -path "*/demos/*")

UI_SRCS   := $(MAIN_DIR)/ui_screens.c \
             $(MAIN_DIR)/lobster_icon.c \
             $(MAIN_DIR)/lobster_icon_lg.c
SIM_SRCS  := sim_main.c

ALL_SRCS  := $(LVGL_SRCS) $(UI_SRCS) $(SIM_SRCS)

# -DCG_SIM       : selects sim stubs in ui_screens.c
# -DLV_CONF_INCLUDE_SIMPLE : tells LVGL to look for lv_conf.h in include path
# -I.            : pick up our lv_conf.h, stb_image_write.h
# -I$(MAIN_DIR)  : pick up ui_screens.h, app_state.h, config.h
# -I$(LVGL_DIR)  : pick up lvgl.h
CFLAGS := -O2 -g -w \
	-DCG_SIM \
	-DLV_CONF_INCLUDE_SIMPLE \
	-I. \
	-I$(MAIN_DIR) \
	-I../components/lvgl \
	-I$(LVGL_DIR) \
	-I$(LVGL_DIR)/src

LDFLAGS := -lm

.PHONY: all build run clean
all: run

build: sim_main

sim_main: $(ALL_SRCS) lv_conf.h sim_main.c
	@mkdir -p out
	$(CC) $(CFLAGS) $(ALL_SRCS) -o $@ $(LDFLAGS)

run: sim_main
	./sim_main

clean:
	rm -f sim_main
	rm -rf out
