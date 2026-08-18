#include "lcd_backlight.h"

#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lcd_backlight_pkg, LOG_LEVEL_INF);

/* Backlight gpio spec from device tree alias. P0.31 drives an external
 * MOSFET gate, not the backlight LEDs directly (see hardware wiring notes). */
#define LCD_BACKLIGHT_NODE  DT_ALIAS(lcd_backlight)

static const struct gpio_dt_spec backlight_spec = GPIO_DT_SPEC_GET(LCD_BACKLIGHT_NODE, gpios);

static int lcd_backlight_init(void)
{
    if (!gpio_is_ready_dt(&backlight_spec)) {
        LOG_ERR("LCD backlight controller not ready!");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&backlight_spec, GPIO_OUTPUT_INACTIVE) < 0) {
        LOG_ERR("LCD backlight pin configuration failed!");
        return -EIO;
    }

    LOG_INF("LCD backlight initialized successfully.");
    return 0;
}

int lcd_backlight_on(void)
{
    return gpio_pin_set_dt(&backlight_spec, 1);
}

int lcd_backlight_off(void)
{
    return gpio_pin_set_dt(&backlight_spec, 0);
}

int lcd_backlight_set(bool on)
{
    return gpio_pin_set_dt(&backlight_spec, on ? 1 : 0);
}

/* Auto-initialize via Zephyr SYS_INIT — no manual lcd_backlight_init() call needed */
SYS_INIT(lcd_backlight_init, APPLICATION, CONFIG_PACKAGE_LCD_BACKLIGHT_INIT_PRIORITY);
