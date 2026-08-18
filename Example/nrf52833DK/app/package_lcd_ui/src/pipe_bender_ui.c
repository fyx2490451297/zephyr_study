#include "pipe_bender_ui.h"
#include "img_assets.h"
#include "fonts.h"

#include <math.h>
#include <lvgl.h>

/* Target angle state: clamped to [0, 180] degrees */
static float s_target_angle = 0.0f;
static const float TARGET_ANGLE_STEP = 0.5f;   /* both short-click step and per-repeat-tick step while held */
static const float TARGET_ANGLE_MIN = 0.0f;
static const float TARGET_ANGLE_MAX = 180.0f;
static const float TARGET_ANGLE_RANGE = 180.0f; /* TARGET_ANGLE_MAX - TARGET_ANGLE_MIN */

static void target_angle_update_label(lv_obj_t *val_tgt)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", (double)s_target_angle);
    lv_label_set_text(val_tgt, buf);
}

/* Adds delta to the target angle, wrapping around at the 0/180 boundary
 * (e.g. 180 + step -> jumps to just above 0, 0 - step -> jumps to just below 180). */
static void target_angle_step(lv_obj_t *val_tgt, float delta)
{
    float angle = fmodf(s_target_angle + delta - TARGET_ANGLE_MIN, TARGET_ANGLE_RANGE);
    if (angle < 0) {
        angle += TARGET_ANGLE_RANGE;
    }
    s_target_angle = angle + TARGET_ANGLE_MIN;
    target_angle_update_label(val_tgt);
}

/* Fires on LV_EVENT_SHORT_CLICKED (single 0.5 deg tap) and LV_EVENT_LONG_PRESSED_REPEAT
 * (continuous 0.5 deg/tick while held down). */
static void target_angle_plus_cb(lv_event_t *e)
{
    lv_obj_t *val_tgt = (lv_obj_t *)lv_event_get_user_data(e);
    target_angle_step(val_tgt, TARGET_ANGLE_STEP);
}

static void target_angle_minus_cb(lv_event_t *e)
{
    lv_obj_t *val_tgt = (lv_obj_t *)lv_event_get_user_data(e);
    target_angle_step(val_tgt, -TARGET_ANGLE_STEP);
}

void pipe_bender_ui_create(void)
{
    /* Screen: logical 320x240 (landscape), very dark background */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* Status bar: 320x30, y=0 */
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 320, 30);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    /* Icon 1: signalLED */
    lv_obj_t *ic_wifi = lv_label_create(sb);
    lv_label_set_text(ic_wifi, ICON_SIGNAL);
    lv_obj_align(ic_wifi, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_text_color(ic_wifi, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ic_wifi, &font_icons_16, 0);
    /* Note: LVGL v8 only supports uniform transform_zoom (no x-only scale
     * like v9's transform_scale_x), so the ~1.15x horizontal stretch from
     * the original UI is intentionally dropped here. */

    /* Icon 2: connectLED */
    lv_obj_t *ic_link = lv_label_create(sb);
    lv_label_set_text(ic_link, ICON_CONNECT);
    lv_obj_align(ic_link, LV_ALIGN_LEFT_MID, 33, 0);
    lv_obj_set_style_text_color(ic_link, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ic_link, &font_icons_16, 0);

    /* Gauge 1 */
    lv_obj_t *ang1_img = lv_img_create(sb);
    lv_img_set_src(ang1_img, &angle_gauge_icon);
    lv_obj_align(ang1_img, LV_ALIGN_LEFT_MID, 62, 0);

    lv_obj_t *bat1_img = lv_img_create(sb);
    lv_img_set_src(bat1_img, &battery_discharging_90p_100p);
    lv_img_set_zoom(bat1_img, 171); /* 2/3x: 22->15px wide, 15->10px tall */
    lv_obj_align(bat1_img, LV_ALIGN_LEFT_MID, 81, -7);

    /* Gauge 2 */
    lv_obj_t *ang2_img = lv_img_create(sb);
    lv_img_set_src(ang2_img, &angle_gauge_icon);
    lv_obj_align(ang2_img, LV_ALIGN_LEFT_MID, 102, 0);

    lv_obj_t *bat2_img = lv_img_create(sb);
    lv_img_set_src(bat2_img, &battery_discharging_0p_10p);
    lv_img_set_zoom(bat2_img, 171);
    lv_obj_align(bat2_img, LV_ALIGN_LEFT_MID, 121, -7);

    /* Separator line */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 320, 1);
    lv_obj_set_pos(sep, 0, 30);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    /* Column headers */
    lv_obj_t *lbl_live = lv_label_create(scr);
    lv_label_set_text(lbl_live, "LIVE ANGLE");
    lv_obj_set_pos(lbl_live, 10, 38);
    lv_obj_set_style_text_color(lbl_live, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_text_font(lbl_live, &lv_font_montserrat_14, 0);

    lv_obj_t *lbl_tgt = lv_label_create(scr);
    lv_label_set_text(lbl_tgt, "TARGET ANGLE");
    lv_obj_set_pos(lbl_tgt, 170, 38);
    lv_obj_set_style_text_color(lbl_tgt, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_text_font(lbl_tgt, &lv_font_montserrat_14, 0);

    /* LIVE ANGLE box */
    lv_obj_t *box_live = lv_obj_create(scr);
    lv_obj_set_size(box_live, 148, 68);
    lv_obj_set_pos(box_live, 8, 56);
    lv_obj_set_style_radius(box_live, 10, 0);
    lv_obj_set_style_bg_color(box_live, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(box_live, 0, 0);
    lv_obj_set_style_pad_all(box_live, 0, 0);
    lv_obj_clear_flag(box_live, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *val_live = lv_label_create(box_live);
    lv_label_set_text(val_live, "0.0\xc2\xb0");
    lv_obj_center(val_live);
    lv_obj_set_style_text_color(val_live, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(val_live, &lv_font_montserrat_26, 0);

    /* TARGET ANGLE box */
    lv_obj_t *box_tgt = lv_obj_create(scr);
    lv_obj_set_size(box_tgt, 148, 68);
    lv_obj_set_pos(box_tgt, 164, 56);
    lv_obj_set_style_radius(box_tgt, 10, 0);
    lv_obj_set_style_bg_color(box_tgt, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(box_tgt, 0, 0);
    lv_obj_set_style_pad_all(box_tgt, 0, 0);
    lv_obj_clear_flag(box_tgt, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *val_tgt = lv_label_create(box_tgt);
    lv_label_set_text(val_tgt, "0.0\xc2\xb0");
    lv_obj_align(val_tgt, LV_ALIGN_CENTER, 16, 0);
    lv_obj_set_style_text_color(val_tgt, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_text_font(val_tgt, &lv_font_montserrat_26, 0);
    target_angle_update_label(val_tgt); /* sync label with initial s_target_angle */

    lv_obj_t *pm_box = lv_obj_create(box_tgt);
    lv_obj_set_size(pm_box, 24, 50);
    lv_obj_align(pm_box, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_radius(pm_box, 5, 0);
    lv_obj_set_style_bg_color(pm_box, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_border_width(pm_box, 0, 0);
    lv_obj_set_style_pad_all(pm_box, 0, 0);
    lv_obj_clear_flag(pm_box, LV_OBJ_FLAG_SCROLLABLE);

    /* '+' button: top half of pm_box, clickable, increments target angle */
    lv_obj_t *btn_plus = lv_obj_create(pm_box);
    lv_obj_set_size(btn_plus, 24, 25);
    lv_obj_align(btn_plus, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(btn_plus, 5, 0);
    lv_obj_set_style_bg_opa(btn_plus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_plus, 0, 0);
    lv_obj_set_style_pad_all(btn_plus, 0, 0);
    lv_obj_clear_flag(btn_plus, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn_plus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_plus, target_angle_plus_cb, LV_EVENT_SHORT_CLICKED, val_tgt);
    lv_obj_add_event_cb(btn_plus, target_angle_plus_cb, LV_EVENT_LONG_PRESSED_REPEAT, val_tgt);

    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, "+");
    lv_obj_align(lbl_plus, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(lbl_plus, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(lbl_plus, LV_OBJ_FLAG_CLICKABLE); /* let clicks pass through to btn_plus */

    /* '-' button: bottom half of pm_box, clickable, decrements target angle */
    lv_obj_t *btn_minus = lv_obj_create(pm_box);
    lv_obj_set_size(btn_minus, 24, 25);
    lv_obj_align(btn_minus, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(btn_minus, 5, 0);
    lv_obj_set_style_bg_opa(btn_minus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_minus, 0, 0);
    lv_obj_set_style_pad_all(btn_minus, 0, 0);
    lv_obj_clear_flag(btn_minus, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn_minus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_minus, target_angle_minus_cb, LV_EVENT_SHORT_CLICKED, val_tgt);
    lv_obj_add_event_cb(btn_minus, target_angle_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT, val_tgt);

    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "-");
    lv_obj_align(lbl_minus, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(lbl_minus, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_text_font(lbl_minus, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(lbl_minus, LV_OBJ_FLAG_CLICKABLE); /* let clicks pass through to btn_minus */

    /* Navigation row */
    lv_obj_t *btn_l = lv_btn_create(scr);
    lv_obj_set_size(btn_l, 36, 30);
    lv_obj_set_pos(btn_l, 8, 134);
    lv_obj_set_style_radius(btn_l, 6, 0);
    lv_obj_set_style_bg_color(btn_l, lv_color_hex(0x1B7A3E), 0);
    lv_obj_set_style_pad_all(btn_l, 0, 0);
    lv_obj_t *lbl_l = lv_label_create(btn_l);
    lv_label_set_text(lbl_l, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_l);
    lv_obj_set_style_text_font(lbl_l, &lv_font_montserrat_14, 0);

    lv_obj_t *btn_bend = lv_btn_create(scr);
    lv_obj_set_size(btn_bend, 100, 30);
    lv_obj_set_pos(btn_bend, 110, 134);
    lv_obj_set_style_radius(btn_bend, 15, 0);
    lv_obj_set_style_bg_color(btn_bend, lv_color_hex(0x1B7A3E), 0);
    lv_obj_set_style_pad_all(btn_bend, 0, 0);
    lv_obj_t *lbl_bend = lv_label_create(btn_bend);
    lv_label_set_text(lbl_bend, "BEND");
    lv_obj_center(lbl_bend);
    lv_obj_set_style_text_font(lbl_bend, &lv_font_montserrat_14, 0);

    lv_obj_t *btn_r = lv_btn_create(scr);
    lv_obj_set_size(btn_r, 36, 30);
    lv_obj_set_pos(btn_r, 276, 134);
    lv_obj_set_style_radius(btn_r, 6, 0);
    lv_obj_set_style_bg_color(btn_r, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_pad_all(btn_r, 0, 0);
    lv_obj_t *lbl_r = lv_label_create(btn_r);
    lv_label_set_text(lbl_r, LV_SYMBOL_RIGHT);
    lv_obj_center(lbl_r);
    lv_obj_set_style_text_font(lbl_r, &lv_font_montserrat_14, 0);

    /* Tip text */
    lv_obj_t *tip = lv_label_create(scr);
    lv_label_set_text(tip, "Press BEND to snug pipe");
    lv_obj_set_pos(tip, 0, 174);
    lv_obj_set_size(tip, 320, 20);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(tip, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(tip, &lv_font_montserrat_12, 0);

    /* HOME button */
    lv_obj_t *btn_home = lv_btn_create(scr);
    lv_obj_set_size(btn_home, 100, 26);
    lv_obj_set_pos(btn_home, 110, 198);
    lv_obj_set_style_radius(btn_home, 6, 0);
    lv_obj_set_style_bg_color(btn_home, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_pad_all(btn_home, 0, 0);
    lv_obj_t *lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, LV_SYMBOL_HOME " HOME");
    lv_obj_center(lbl_home);
    lv_obj_set_style_text_font(lbl_home, &lv_font_montserrat_14, 0);
}
