#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <lvgl.h>

LOG_MODULE_REGISTER(lvgl_demo, LOG_LEVEL_INF);

#define LVGL_DEMO_DISPLAY_NODE DT_NODELABEL(ili9341)

#if !DT_NODE_HAS_STATUS(LVGL_DEMO_DISPLAY_NODE, okay)
#error "package_lvgl_demo requires the ili9341 display node to be enabled in device tree"
#endif

/* Uptime counter is refreshed once a second regardless of the LVGL tick rate. */
#define LVGL_DEMO_COUNTER_PERIOD_MS 1000

static const struct device *const display_dev = DEVICE_DT_GET(LVGL_DEMO_DISPLAY_NODE);

static lv_obj_t *counter_label;

static void lvgl_demo_build_ui(void)
{
	lv_obj_t *title_label = lv_label_create(lv_scr_act());

	lv_label_set_text(title_label, "nRF52833DK Pendant");
	lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

	counter_label = lv_label_create(lv_scr_act());
	lv_label_set_text(counter_label, "Uptime: 0 s");
	lv_obj_align(counter_label, LV_ALIGN_CENTER, 0, 0);
}

static void lvgl_demo_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	char text_buf[32];
	uint32_t uptime_s = 0;
	int32_t counter_due_ms = 0;

	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return;
	}

	lvgl_demo_build_ui();
	display_blanking_off(display_dev);

	LOG_INF("LVGL demo started on %s", display_dev->name);

	while (1) {
		uint32_t lvgl_sleep_ms = lv_timer_handler();
		uint32_t sleep_ms = MIN(lvgl_sleep_ms, CONFIG_PACKAGE_LVGL_DEMO_REFRESH_MS);

		counter_due_ms -= (int32_t)sleep_ms;
		if (counter_due_ms <= 0) {
			snprintf(text_buf, sizeof(text_buf), "Uptime: %u s", uptime_s);
			lv_label_set_text(counter_label, text_buf);
			uptime_s++;
			counter_due_ms = LVGL_DEMO_COUNTER_PERIOD_MS;
		}

		k_sleep(K_MSEC(sleep_ms));
	}
}

K_THREAD_DEFINE(lvgl_demo_tid,
		CONFIG_PACKAGE_LVGL_DEMO_STACK_SIZE,
		lvgl_demo_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_LVGL_DEMO_PRIORITY,
		0, 0);
