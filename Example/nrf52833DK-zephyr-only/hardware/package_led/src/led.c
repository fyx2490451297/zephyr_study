#include "led.h"
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_pkg, LOG_LEVEL_INF);

/* LED gpio specs from device tree aliases */
#define LED1_NODE   DT_ALIAS(led1)
#define LED2_NODE   DT_ALIAS(led2)
#define LED3_NODE   DT_ALIAS(led3)
#define LED4_NODE   DT_ALIAS(led4)

static const struct gpio_dt_spec led_specs[LED_MAX] = {
    [LED1] = GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    [LED2] = GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    [LED3] = GPIO_DT_SPEC_GET(LED3_NODE, gpios),
    [LED4] = GPIO_DT_SPEC_GET(LED4_NODE, gpios),
};

static int led_init(void)
{
    for (int i = 0; i < LED_MAX; i++) {
        if (!gpio_is_ready_dt(&led_specs[i])) {
            LOG_ERR("LED%d controller not ready!", i + 1);
            return -ENODEV;
        }
        if (gpio_pin_configure_dt(&led_specs[i], GPIO_OUTPUT_INACTIVE) < 0) {
            LOG_ERR("LED%d pin configuration failed!", i + 1);
            return -EIO;
        }
    }

    LOG_INF("All LEDs initialized successfully.");
    return 0;
}

int led_on(led_id_t led_id)
{
    if (led_id >= LED_MAX) {
        LOG_ERR("Invalid LED ID: %d", led_id);
        return -EINVAL;
    }
    return gpio_pin_set_dt(&led_specs[led_id], 1);
}

int led_off(led_id_t led_id)
{
    if (led_id >= LED_MAX) {
        LOG_ERR("Invalid LED ID: %d", led_id);
        return -EINVAL;
    }
    return gpio_pin_set_dt(&led_specs[led_id], 0);
}

int led_toggle(led_id_t led_id)
{
    if (led_id >= LED_MAX) {
        LOG_ERR("Invalid LED ID: %d", led_id);
        return -EINVAL;
    }
    return gpio_pin_toggle_dt(&led_specs[led_id]);
}

/* Auto-initialize via Zephyr SYS_INIT — no manual led_init() call needed */
SYS_INIT(led_init, APPLICATION, CONFIG_PACKAGE_LED_INIT_PRIORITY);