#include "led.h"
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_pkg, LOG_LEVEL_INF);

/* LED nodes from device tree */
#define LED1_NODE       DT_ALIAS(led1)
#define LED2_NODE       DT_ALIAS(led2)
#define LED3_NODE       DT_ALIAS(led3)
#define LED4_NODE       DT_ALIAS(led4)
static const struct gpio_dt_spec led1_spec = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2_spec = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3_spec = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
static const struct gpio_dt_spec led4_spec = GPIO_DT_SPEC_GET(LED4_NODE, gpios);

static struct k_thread led_thread_data;
K_THREAD_STACK_DEFINE(led_thread_stack, 1024);

int led_init(void)
{
    const struct gpio_dt_spec *leds[] = { &led1_spec, &led2_spec, &led3_spec, &led4_spec };

    for (size_t i = 0; i < LED_MAX; i++) {
        if (!gpio_is_ready_dt(leds[i])) {
            LOG_ERR("LED%d controller not ready!", i + 1);
            return -ENODEV;
        }
        if (gpio_pin_configure_dt(leds[i], GPIO_OUTPUT_INACTIVE) < 0) {
            LOG_ERR("LED%d pin configuration failed!", i + 1);
            return -EIO;
        }
    }

    LOG_INF("All LEDs initialized successfully.");
    return 0;
}

int led_on(led_id_t led_id)
{
    const struct gpio_dt_spec *leds[] = { &led1_spec, &led2_spec, &led3_spec, &led4_spec };

    if (led_id >= LED_MAX) {
        LOG_ERR("Invalid LED ID: %d", led_id);
        return -EINVAL;
    }
    return gpio_pin_set_dt(leds[led_id], 1);
}

int led_off(led_id_t led_id)
{
    const struct gpio_dt_spec *leds[] = { &led1_spec, &led2_spec, &led3_spec, &led4_spec };

    if (led_id >= LED_MAX) {
        LOG_ERR("Invalid LED ID: %d", led_id);
        return -EINVAL;
    }
    return gpio_pin_set_dt(leds[led_id], 0);
}

int led_toggle(led_id_t led_id)
{
    const struct gpio_dt_spec *leds[] = { &led1_spec, &led2_spec, &led3_spec, &led4_spec };

    if (led_id >= LED_MAX) {
        LOG_ERR("Invalid LED ID: %d", led_id);
        return -EINVAL;
    }
    return gpio_pin_toggle_dt(leds[led_id]);
}
