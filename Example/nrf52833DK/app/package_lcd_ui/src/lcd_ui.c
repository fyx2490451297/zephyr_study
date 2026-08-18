#include "pipe_bender_ui.h"
#include "lcd_backlight.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(lcd_ui, LOG_LEVEL_INF);

static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static void lcd_ui_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device %s not ready!", display_dev->name);
        return;
    }

    pipe_bender_ui_create();

    /* Render the first frame before turning the backlight/blanking on,
     * so the panel doesn't flash stale/garbage content. */
    lv_task_handler();
    display_blanking_off(display_dev);
    lcd_backlight_on();

    LOG_INF("LCD UI thread started.");

    while (1) {
        lv_task_handler();
        k_sleep(K_MSEC(10));
    }
}

K_THREAD_DEFINE(lcd_ui_tid,
                CONFIG_PACKAGE_LCD_UI_STACK_SIZE,
                lcd_ui_thread,
                NULL, NULL, NULL,
                CONFIG_PACKAGE_LCD_UI_PRIORITY,
                0, 0);
