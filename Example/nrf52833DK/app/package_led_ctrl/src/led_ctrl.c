#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "led.h"

LOG_MODULE_REGISTER(led_ctrl, LOG_LEVEL_INF);

static void led_blink_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("LED blink thread started.");

    while (1) {
        led_toggle(LED1);
        k_sleep(K_SECONDS(1));
    }
}

K_THREAD_DEFINE(led_blink_tid,
                CONFIG_PACKAGE_LED_CTRL_STACK_SIZE,
                led_blink_thread,
                NULL, NULL, NULL,
                CONFIG_PACKAGE_LED_CTRL_PRIORITY,
                0, 0);
