#include "display.h"

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(display_pkg, LOG_LEVEL_INF);

#if !DT_HAS_CHOSEN(zephyr_display)
#error "package_display requires a zephyr,display chosen node in device tree"
#endif

/*
 * This is the only place in the codebase allowed to know which concrete
 * panel driver is bound to the devicetree "zephyr,display" chosen node.
 * Every other layer (LVGL port, UI) must go through this package's API.
 */
static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static bool display_ready;

bool display_is_ready(void)
{
	return display_ready;
}

int display_power_on(void)
{
	if (!display_ready) {
		return -EAGAIN;
	}
	return display_blanking_off(display_dev);
}

int display_power_off(void)
{
	if (!display_ready) {
		return -EAGAIN;
	}
	return display_blanking_on(display_dev);
}

static int display_init(void)
{
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device (chosen zephyr,display) not ready");
		return -ENODEV;
	}

	display_ready = true;
	LOG_INF("Display initialized on %s", display_dev->name);
	return 0;
}

SYS_INIT(display_init, APPLICATION, CONFIG_PACKAGE_DISPLAY_INIT_PRIORITY);
