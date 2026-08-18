#ifndef LCD_UI_IMG_ASSETS_H_
#define LCD_UI_IMG_ASSETS_H_

#include <lvgl.h>

/* Battery + angle gauge icon assets, converted from the original LVGL v9
 * ARGB8888 sources to LVGL v8 LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 + 8-bit
 * alpha) format. See src/img_assets/*.c. */
LV_IMG_DECLARE(angle_gauge_icon);
LV_IMG_DECLARE(battery_discharging_0p_10p);
LV_IMG_DECLARE(battery_discharging_10p_25p);
LV_IMG_DECLARE(battery_discharging_25p_50p);
LV_IMG_DECLARE(battery_discharging_50p_90p);
LV_IMG_DECLARE(battery_discharging_90p_100p);
LV_IMG_DECLARE(battery_charging_0p_25p);
LV_IMG_DECLARE(battery_charging_25p_90p);
LV_IMG_DECLARE(battery_charging_90p_100p);
LV_IMG_DECLARE(battery_charging_not);

#endif /* LCD_UI_IMG_ASSETS_H_ */
