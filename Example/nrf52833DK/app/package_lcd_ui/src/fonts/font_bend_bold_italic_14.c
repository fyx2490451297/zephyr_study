/* Bold-italic "BEND" glyphs (B,E,N,D), 14px, 4bpp. Generated from
 * LiberationSans-BoldItalic.ttf via lv_font_conv; used only by the BEND
 * button label so it can be genuinely bold+italic without relying on
 * LVGL's runtime rotation transform (unreliable on this display's small
 * partial render buffer). */
#include <lvgl.h>

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t font_bend_bold_italic_14_bitmap[] = {
    /* U+0042 "B" */
    0x0, 0xff, 0xff, 0xfd, 0x70, 0x2, 0xfe, 0x99,
    0xaf, 0xf6, 0x5, 0xfa, 0x0, 0x7, 0xf8, 0x8,
    0xf7, 0x0, 0x2c, 0xf4, 0xb, 0xff, 0xff, 0xfd,
    0x40, 0xe, 0xf9, 0x99, 0xcf, 0xb0, 0x1f, 0xe0,
    0x0, 0xc, 0xf4, 0x4f, 0xb0, 0x0, 0xd, 0xf4,
    0x7f, 0xd9, 0x9a, 0xdf, 0xd0, 0xaf, 0xff, 0xff,
    0xe9, 0x10,

    /* U+0044 "D" */
    0x0, 0xff, 0xff, 0xd8, 0x0, 0x2, 0xfe, 0xab,
    0xef, 0xc0, 0x5, 0xfa, 0x0, 0x1c, 0xf6, 0x8,
    0xf7, 0x0, 0x5, 0xfa, 0xb, 0xf4, 0x0, 0x3,
    0xfa, 0xe, 0xf1, 0x0, 0x5, 0xf9, 0x1f, 0xe0,
    0x0, 0xa, 0xf4, 0x4f, 0xb0, 0x0, 0x7f, 0xd0,
    0x7f, 0xda, 0xbe, 0xfd, 0x20, 0xaf, 0xff, 0xfd,
    0x81, 0x0,

    /* U+0045 "E" */
    0x0, 0xff, 0xff, 0xff, 0xf9, 0x2, 0xfe, 0xaa,
    0xaa, 0xa4, 0x5, 0xfa, 0x0, 0x0, 0x0, 0x8,
    0xf7, 0x0, 0x0, 0x0, 0xb, 0xff, 0xff, 0xff,
    0x60, 0xe, 0xfa, 0xaa, 0xaa, 0x20, 0x1f, 0xe0,
    0x0, 0x0, 0x0, 0x4f, 0xb0, 0x0, 0x0, 0x0,
    0x7f, 0xda, 0xaa, 0xaa, 0x30, 0xaf, 0xff, 0xff,
    0xff, 0x30,

    /* U+004E "N" */
    0x0, 0xff, 0xa0, 0x0, 0x8f, 0x30, 0x2f, 0xff,
    0x0, 0xb, 0xf1, 0x5, 0xfc, 0xf4, 0x0, 0xee,
    0x0, 0x8f, 0x4f, 0xa0, 0x1f, 0xb0, 0xb, 0xf1,
    0xbe, 0x4, 0xf8, 0x0, 0xee, 0x6, 0xf4, 0x7f,
    0x50, 0x1f, 0xb0, 0x1f, 0x9a, 0xf2, 0x4, 0xf8,
    0x0, 0xce, 0xdf, 0x0, 0x7f, 0x50, 0x6, 0xff,
    0xc0, 0xa, 0xf2, 0x0, 0x1f, 0xf9, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t font_bend_bold_italic_14_glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 162, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 50, .adv_w = 162, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 149, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 162, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t font_bend_bold_italic_14_unicode_list_0[] = {
    0x0, 0x2, 0x3, 0xc
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t font_bend_bold_italic_14_cmaps[] =
{
    {
        .range_start = 66, .range_length = 13, .glyph_id_start = 1,
        .unicode_list = font_bend_bold_italic_14_unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t font_bend_bold_italic_14_cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_bend_bold_italic_14_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_bend_bold_italic_14_dsc = {
#endif
    .glyph_bitmap = font_bend_bold_italic_14_bitmap,
    .glyph_dsc = font_bend_bold_italic_14_glyph_dsc,
    .cmaps = font_bend_bold_italic_14_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &font_bend_bold_italic_14_cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_bend_bold_italic_14 = {
#else
lv_font_t font_bend_bold_italic_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 10,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 1,
#endif
    .dsc = &font_bend_bold_italic_14_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



