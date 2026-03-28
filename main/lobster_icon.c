/*******************************************************************************
 * Size: 16 px
 * Bpp: 4
 * Opts: --font /tmp/NotoEmojiVar.ttf --range 0x1F99E --size 16 --bpp 4 --format lvgl -o /Users/david/Documents/GitHub/esp32-s3-3_5inch-001-sdk-master/examples/clawglance/main/lobster_icon.c --no-compress
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LOBSTER_ICON
#define LOBSTER_ICON 1
#endif

#if LOBSTER_ICON

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+1F99E "🦞" */
    0x5, 0x40, 0x18, 0x20, 0x65, 0x0, 0x81, 0x3,
    0x68, 0x85, 0x39, 0xa, 0x64, 0x93, 0xa0, 0x90,
    0xa7, 0x20, 0xa0, 0xa0, 0xa8, 0x15, 0x49, 0x8,
    0x54, 0xa, 0xa, 0xb, 0x33, 0x17, 0x90, 0x17,
    0x20, 0xa0, 0xa0, 0xb0, 0x0, 0x9a, 0x0, 0xa0,
    0xd, 0xca, 0x7, 0x40, 0x27, 0x93, 0x58, 0x1,
    0xf5, 0xe0, 0x1d, 0x9, 0x20, 0x9f, 0xd0, 0x91,
    0xa, 0x33, 0xfc, 0x50, 0x0, 0x7f, 0xd8, 0x82,
    0x8f, 0xfa, 0x0, 0x0, 0x28, 0x1b, 0x70, 0x1,
    0xf4, 0x47, 0x0, 0x0, 0x17, 0xa8, 0x0, 0x2f,
    0x85, 0x0, 0x0, 0x8, 0x8b, 0xa0, 0x3, 0xe9,
    0x92, 0x0, 0x0, 0x39, 0x7f, 0x99, 0xba, 0x77,
    0x10, 0x0, 0x3, 0x74, 0xd8, 0x9b, 0x78, 0x50,
    0x0, 0x0, 0x3, 0xa6, 0x10, 0x29, 0x71, 0x0,
    0x0, 0x0, 0xa, 0x11, 0x2, 0x19, 0x0, 0x0,
    0x0, 0x0, 0x58, 0x89, 0x79, 0x20, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 325, .box_w = 15, .box_h = 17, .ofs_x = 3, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 129438, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lobster_icon = {
#else
lv_font_t lobster_icon = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 10,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LOBSTER_ICON*/

