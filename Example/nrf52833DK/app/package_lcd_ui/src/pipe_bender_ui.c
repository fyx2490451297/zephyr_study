#include "pipe_bender_ui.h"
#include "img_assets.h"
#include "fonts.h"

#include <math.h>
#include <lvgl.h>
#include <zephyr/toolchain.h>

/* Target angle state: clamped to [0, 180] degrees */
static float s_target_angle = 0.0f;
static const float TARGET_ANGLE_STEP = 0.5f;   /* both short-click step and per-repeat-tick step while held */
static const float TARGET_ANGLE_MIN = 0.0f;
static const float TARGET_ANGLE_MAX = 180.0f;
static const float TARGET_ANGLE_RANGE = 180.0f; /* TARGET_ANGLE_MAX - TARGET_ANGLE_MIN */

/* --- Live-usage simulation demo ---
 * Periodically sweeps the target angle back and forth across [0, 180] and
 * drives the live angle toward it with a lag, mimicking a real motor/sensor
 * closing the gap on a moving target. Purely cosmetic; no real hardware
 * feedback is involved. */
static lv_obj_t *s_val_live;
static lv_obj_t *s_val_tgt;
static lv_obj_t *s_val_live_bold; /* faux-bold overlay for the live angle value */
static lv_obj_t *s_val_tgt_bold;  /* faux-bold overlay for the target angle value */
static float s_live_angle = 0.0f;
static int8_t s_demo_dir = 1; /* +1 sweeping up, -1 sweeping down */
static lv_timer_t *s_demo_timer;

#define DEMO_TIMER_PERIOD_MS   50
#define DEMO_TARGET_STEP_DEG   1.0f   /* target sweep speed: ~20 deg/s */
#define DEMO_LIVE_LERP         0.12f  /* live angle catch-up rate per tick */

/* --- Faux bold text styling ---
 * LVGL v8's built-in fonts ship a single (regular) weight only, so there is
 * no font-weight style property to flip. Bold is approximated with the
 * classic "1px-offset duplicate" trick: a second label with identical
 * text/color/font is stacked 1px to the right of the original, thickening
 * every glyph.
 *
 * The BEND button is the one exception: it needs to be BOTH bold and
 * italic, and LVGL's generic widget rotation transform
 * (LV_STYLE_TRANSFORM_ANGLE) turned out to be unreliable on this display's
 * small partial render buffer — it intermittently failed to allocate its
 * offscreen transform layer, making the BEND text vanish entirely. Instead
 * BEND uses a real bold-italic bitmap font (font_bend_bold_italic_14,
 * generated from LiberationSans-BoldItalic.ttf) so no runtime transform is
 * needed at all. */
static lv_obj_t *label_make_bold(lv_obj_t *label)
{
    lv_obj_t *ghost = lv_label_create(lv_obj_get_parent(label));
    lv_label_set_text(ghost, lv_label_get_text(label));
    lv_obj_set_style_text_font(ghost, lv_obj_get_style_text_font(label, LV_PART_MAIN), 0);
    lv_obj_set_style_text_color(ghost, lv_obj_get_style_text_color(label, LV_PART_MAIN), 0);
    lv_obj_set_style_text_align(ghost, lv_obj_get_style_text_align(label, LV_PART_MAIN), 0);
    /* Match the original's box size/wrap mode too, otherwise a fixed-width
     * wrapping label (e.g. the tip text) re-wraps differently at its
     * default auto-size and shows up as a visible extra line underneath. */
    lv_label_set_long_mode(ghost, lv_label_get_long_mode(label));
    lv_obj_set_size(ghost, lv_obj_get_width(label), lv_obj_get_height(label));
    lv_obj_clear_flag(ghost, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align_to(ghost, label, LV_ALIGN_TOP_LEFT, 1, 0);
    return ghost;
}

static void target_angle_update_label(lv_obj_t *val_tgt)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", (double)s_target_angle);
    lv_label_set_text(val_tgt, buf);
    if (s_val_tgt_bold != NULL) {
        lv_label_set_text(s_val_tgt_bold, buf);
    }
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

static void live_angle_update_label(void)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", (double)s_live_angle);
    lv_label_set_text(s_val_live, buf);
    if (s_val_live_bold != NULL) {
        lv_label_set_text(s_val_live_bold, buf);
    }
}

/* --- Bluetooth / link status icon breathing animation ---
 * Fades the icon's text opacity up and down forever to give a "breathing
 * LED" look. Both status icons share the same blue tint and timing. */
#define ICON_BREATH_COLOR       0x2E9EFF /* blue */
#define ICON_BREATH_TIME_MS     1200     /* fade-in/out duration each way */
#define ICON_BREATH_OPA_MIN     LV_OPA_30

static void icon_breath_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void icon_start_breath_anim(lv_obj_t *icon)
{
    lv_obj_set_style_text_color(icon, lv_color_hex(ICON_BREATH_COLOR), 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, icon);
    lv_anim_set_exec_cb(&a, icon_breath_anim_cb);
    lv_anim_set_values(&a, ICON_BREATH_OPA_MIN, LV_OPA_COVER);
    lv_anim_set_time(&a, ICON_BREATH_TIME_MS);
    lv_anim_set_playback_time(&a, ICON_BREATH_TIME_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* Advances the auto-demo one tick: bounces the target angle between 0 and
 * 180 degrees, then lets the live angle chase it with an exponential
 * catch-up (lag), similar to how a real motor angle would approach a
 * moving setpoint. */
static void demo_sim_timer_cb(lv_timer_t *timer)
{
    ARG_UNUSED(timer);

    s_target_angle += s_demo_dir * DEMO_TARGET_STEP_DEG;
    if (s_target_angle >= TARGET_ANGLE_MAX) {
        s_target_angle = TARGET_ANGLE_MAX;
        s_demo_dir = -1;
    } else if (s_target_angle <= TARGET_ANGLE_MIN) {
        s_target_angle = TARGET_ANGLE_MIN;
        s_demo_dir = 1;
    }

    s_live_angle += (s_target_angle - s_live_angle) * DEMO_LIVE_LERP;
    if (s_live_angle < TARGET_ANGLE_MIN) {
        s_live_angle = TARGET_ANGLE_MIN;
    } else if (s_live_angle > TARGET_ANGLE_MAX) {
        s_live_angle = TARGET_ANGLE_MAX;
    }

    target_angle_update_label(s_val_tgt);
    live_angle_update_label();
}

void pipe_bender_ui_create(void)
{
    /* Reset any stale bold-overlay pointers from a previous create() call
     * before wiring up the fresh ones below. */
    s_val_live_bold = NULL;
    s_val_tgt_bold = NULL;

    /* Screen: logical 320x240 (landscape), very dark background */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* Status bar background: 320x30 gray panel, moved to the bottom of the screen */
    lv_obj_t *sb_bg = lv_obj_create(scr);
    lv_obj_set_size(sb_bg, 320, 30);
    lv_obj_set_pos(sb_bg, 0, 210);
    lv_obj_set_style_bg_color(sb_bg, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(sb_bg, 0, 0);
    lv_obj_set_style_pad_all(sb_bg, 0, 0);
    lv_obj_set_style_radius(sb_bg, 0, 0);
    lv_obj_clear_flag(sb_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* Status icon row: transparent container kept at the top (y=0) so the
     * wifi/link/gauge/battery icons stay in place independent of where the
     * gray background panel is. */
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 320, 30);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    /* Icon 1: signalLED */
    lv_obj_t *ic_wifi = lv_label_create(sb);
    lv_label_set_text(ic_wifi, ICON_SIGNAL);
    lv_obj_align(ic_wifi, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_text_font(ic_wifi, &font_icons_16, 0);
    icon_start_breath_anim(ic_wifi); /* blue breathing effect */
    /* Note: LVGL v8 only supports uniform transform_zoom (no x-only scale
     * like v9's transform_scale_x), so the ~1.15x horizontal stretch from
     * the original UI is intentionally dropped here. */

    /* Icon 2: connectLED */
    lv_obj_t *ic_link = lv_label_create(sb);
    lv_label_set_text(ic_link, ICON_CONNECT);
    lv_obj_align(ic_link, LV_ALIGN_LEFT_MID, 33, 0);
    lv_obj_set_style_text_font(ic_link, &font_icons_16, 0);
    icon_start_breath_anim(ic_link); /* blue breathing effect */

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

    /* Separator line: sits just above the status bar now that it's at the bottom */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 320, 1);
    lv_obj_set_pos(sep, 0, 209);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    /* Column headers */
    lv_obj_t *lbl_live = lv_label_create(scr);
    lv_label_set_text(lbl_live, "LIVE ANGLE");
    lv_obj_set_pos(lbl_live, 10, 34);
    lv_obj_set_style_text_color(lbl_live, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_text_font(lbl_live, &lv_font_montserrat_14, 0);
    label_make_bold(lbl_live);

    lv_obj_t *lbl_tgt = lv_label_create(scr);
    lv_label_set_text(lbl_tgt, "TARGET ANGLE");
    lv_obj_set_pos(lbl_tgt, 170, 34);
    lv_obj_set_style_text_color(lbl_tgt, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_text_font(lbl_tgt, &lv_font_montserrat_14, 0);
    label_make_bold(lbl_tgt);

    /* LIVE ANGLE box */
    lv_obj_t *box_live = lv_obj_create(scr);
    lv_obj_set_size(box_live, 148, 68);
    lv_obj_set_pos(box_live, 8, 50);
    lv_obj_set_style_radius(box_live, 10, 0);
    lv_obj_set_style_bg_color(box_live, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(box_live, 0, 0);
    lv_obj_set_style_pad_all(box_live, 0, 0);
    lv_obj_clear_flag(box_live, LV_OBJ_FLAG_SCROLLABLE);

    s_val_live = lv_label_create(box_live);
    lv_label_set_text(s_val_live, "0.0\xc2\xb0");
    lv_obj_center(s_val_live);
    lv_obj_set_style_text_color(s_val_live, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(s_val_live, &lv_font_montserrat_26, 0);
    s_val_live_bold = label_make_bold(s_val_live);

    /* TARGET ANGLE box */
    lv_obj_t *box_tgt = lv_obj_create(scr);
    lv_obj_set_size(box_tgt, 148, 68);
    lv_obj_set_pos(box_tgt, 164, 50);
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
    s_val_tgt = val_tgt;
    target_angle_update_label(val_tgt); /* sync label with initial s_target_angle */
    s_val_tgt_bold = label_make_bold(val_tgt);

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
    label_make_bold(lbl_plus);

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
    label_make_bold(lbl_minus);

    /* Navigation row */
    lv_obj_t *btn_l = lv_btn_create(scr);
    lv_obj_set_size(btn_l, 36, 30);
    lv_obj_set_pos(btn_l, 8, 126);
    lv_obj_set_style_radius(btn_l, 6, 0);
    lv_obj_set_style_bg_color(btn_l, lv_color_hex(0x1B7A3E), 0);
    lv_obj_set_style_pad_all(btn_l, 0, 0);
    lv_obj_t *lbl_l = lv_label_create(btn_l);
    lv_label_set_text(lbl_l, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_l);
    lv_obj_set_style_text_font(lbl_l, &lv_font_montserrat_14, 0);
    label_make_bold(lbl_l);

    lv_obj_t *btn_bend = lv_btn_create(scr);
    lv_obj_set_size(btn_bend, 100, 30);
    lv_obj_set_pos(btn_bend, 110, 126);
    lv_obj_set_style_radius(btn_bend, 15, 0);
    lv_obj_set_style_bg_color(btn_bend, lv_color_hex(0x1B7A3E), 0);
    lv_obj_set_style_pad_all(btn_bend, 0, 0);
    lv_obj_t *lbl_bend = lv_label_create(btn_bend);
    lv_label_set_text(lbl_bend, "BEND");
    lv_obj_center(lbl_bend);
    lv_obj_set_style_text_font(lbl_bend, &font_bend_bold_italic_14, 0); /* real bold-italic glyphs */

    lv_obj_t *btn_r = lv_btn_create(scr);
    lv_obj_set_size(btn_r, 36, 30);
    lv_obj_set_pos(btn_r, 276, 126);
    lv_obj_set_style_radius(btn_r, 6, 0);
    lv_obj_set_style_bg_color(btn_r, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_pad_all(btn_r, 0, 0);
    lv_obj_t *lbl_r = lv_label_create(btn_r);
    lv_label_set_text(lbl_r, LV_SYMBOL_RIGHT);
    lv_obj_center(lbl_r);
    lv_obj_set_style_text_font(lbl_r, &lv_font_montserrat_14, 0);
    label_make_bold(lbl_r);

    /* Tip text */
    lv_obj_t *tip = lv_label_create(scr);
    lv_label_set_text(tip, "Press BEND to snug pipe");
    lv_obj_set_pos(tip, 0, 170);
    lv_obj_set_size(tip, 320, 20);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(tip, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(tip, &lv_font_montserrat_12, 0);
    label_make_bold(tip);

    /* HOME button: lives inside the bottom gray status bar (sb_bg) instead
     * of the main content area, centered in its 30px height. */
    lv_obj_t *btn_home = lv_btn_create(sb_bg);
    lv_obj_set_size(btn_home, 100, 24);
    lv_obj_align(btn_home, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(btn_home, 6, 0);
    lv_obj_set_style_bg_color(btn_home, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_pad_all(btn_home, 0, 0);
    lv_obj_t *lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, LV_SYMBOL_HOME " HOME");
    lv_obj_center(lbl_home);
    lv_obj_set_style_text_font(lbl_home, &lv_font_montserrat_14, 0);
    label_make_bold(lbl_home);

    /* Start the live/target angle auto-demo. Guard against a stale timer if
     * this screen is ever rebuilt (e.g. re-entering the pipe-bender view). */
    if (s_demo_timer != NULL) {
        lv_timer_del(s_demo_timer);
    }
    s_target_angle = TARGET_ANGLE_MIN;
    s_live_angle = TARGET_ANGLE_MIN;
    s_demo_dir = 1;
    target_angle_update_label(s_val_tgt);
    live_angle_update_label();
    s_demo_timer = lv_timer_create(demo_sim_timer_cb, DEMO_TIMER_PERIOD_MS, NULL);
}
