#include "app_led.h"
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pkg_led, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec system_led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* ---------- 后台闪烁定时器 (条件编译) ---------- */
#ifdef CONFIG_PACKAGE_LED_AUTO_BLINK

static void led_timer_handler(struct k_timer *dummy)
{
    gpio_pin_toggle_dt(&system_led);
}

K_TIMER_DEFINE(led_blink_timer, led_timer_handler, NULL);

#endif /* CONFIG_PACKAGE_LED_AUTO_BLINK */

/* ---------- 暴露的 API 实现 ---------- */
int app_led_on(void)   { return gpio_pin_set_dt(&system_led, 1); }
int app_led_off(void)  { return gpio_pin_set_dt(&system_led, 0); }
int app_led_toggle(void){ return gpio_pin_toggle_dt(&system_led); }

/* ---------- 内核自动初始化机制 ---------- */
static int package_led_init(void)
{
    if (!gpio_is_ready_dt(&system_led)) {
        LOG_ERR("LED 控制器未就绪！");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&system_led, GPIO_OUTPUT_INACTIVE) < 0) {
        LOG_ERR("LED 引脚配置失败！");
        return -EIO;
    }

#ifdef CONFIG_PACKAGE_LED_AUTO_BLINK
    k_timeout_t interval = K_MSEC(CONFIG_PACKAGE_LED_BLINK_INTERVAL);
    k_timer_start(&led_blink_timer, interval, interval);
    LOG_INF("LED 后台自动闪烁已启动，间隔: %d ms", CONFIG_PACKAGE_LED_BLINK_INTERVAL);
#else
    LOG_INF("LED 模块初始化完成 (手动控制模式)。");
#endif

    return 0;
}

/* 优先级设为 90，确保在系统应用阶段早期完成初始化 */
SYS_INIT(package_led_init, APPLICATION, 90);