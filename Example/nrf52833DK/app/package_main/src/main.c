#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Hardware */
#include "led.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_DBG);

int main(void)
{
    LOG_INF("Hello, Zephyr on nRF52833DK!");

    /* Initialize hardware */
    if (led_init() != 0) {
        LOG_ERR("Failed to initialize LEDs");
        return -1;
    }

    while (1) {
        LOG_DBG("Main loop is running...");
        k_sleep(K_SECONDS(2));
    }

    /* This point should never be reached */
    return 0;
}