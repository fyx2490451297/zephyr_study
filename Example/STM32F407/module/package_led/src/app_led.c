#include "app_led.h"
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(pkg_led, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec system_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* ---------- Background LED blink timer (conditional compilation) ---------- */
#ifdef CONFIG_PACKAGE_LED_AUTO_BLINK

static void led_timer_handler(struct k_timer *dummy)
{
    gpio_pin_toggle_dt(&system_led);
}

K_TIMER_DEFINE(led_blink_timer, led_timer_handler, NULL);

#endif /* CONFIG_PACKAGE_LED_AUTO_BLINK */

/* ---------- Public LED control API ---------- */
int app_led_on(void)            { return gpio_pin_set_dt(&system_led, 1); }
int app_led_off(void)           { return gpio_pin_set_dt(&system_led, 0); }
int app_led_toggle(void)        { return gpio_pin_toggle_dt(&system_led); }

/* ---------- Kernel auto-initialization mechanism ---------- */
static int package_led_init(void)
{
    if (!gpio_is_ready_dt(&system_led)) {
        LOG_ERR("LED controller not ready!");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&system_led, GPIO_OUTPUT_INACTIVE) < 0) {
        LOG_ERR("LED pin configuration failed!");
        return -EIO;
    }

#ifdef CONFIG_PACKAGE_LED_AUTO_BLINK
    k_timeout_t interval = K_MSEC(CONFIG_PACKAGE_LED_BLINK_INTERVAL);
    k_timer_start(&led_blink_timer, interval, interval);
    LOG_INF("LED background auto-blink started, interval: %d ms", CONFIG_PACKAGE_LED_BLINK_INTERVAL);
#else
    LOG_INF("LED module initialized (manual control mode).");
#endif

    return 0;
}

/* Priority set to 90 to ensure early initialization in the application phase */
SYS_INIT(package_led_init, APPLICATION, 90);