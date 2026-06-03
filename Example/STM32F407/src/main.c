#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(blinky_demo, LOG_LEVEL_DBG);

/* 1. 从设备树获取 blue_led 别名对应的硬件信息 */
#define BLUE_LED    DT_ALIAS(led0)
#define RED_LED     DT_ALIAS(led1)

/* 2. 将设备树节点转换为代码里可以使用的 GPIO 规格体 */
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED, gpios);
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED, gpios);

int main(void)
{
    LOG_INF("STM32F407 Blinky Demo Started!");

    /* 3. 检查设备树里配置的 GPIO 控制器是否已经就绪 */
    if (!gpio_is_ready_dt(&blue_led) || !gpio_is_ready_dt(&red_led)) {
        LOG_ERR("Error: GPIO device is not ready!");
        return 0;
    }

    /* 4. 配置引脚为输出模式
     * 注意：在 overlay 里写了 ACTIVE_LOW，这里 Zephyr 会自动处理极性，
     * 只需要关心“逻辑上”的初始化。
     */
    if ( gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE) < 0 ||\
         gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ) {
        LOG_ERR("Error: Failed to configure GPIO pin!");
        return 0;
    }

    /* 5. 翻转电平 */
    while (1) {
        /* 翻转引脚电平 */
        gpio_pin_toggle_dt(&blue_led);
        gpio_pin_toggle_dt(&red_led);
        LOG_INF("LED toggled!");

        k_msleep(500);
    }
    return 0;
}