#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_DBG);

int main(void)
{
    LOG_INF("Hello, Zephyr on nRF52833DK!");

    while (1) {
        LOG_DBG("Main loop is running...");
        k_sleep(K_SECONDS(2));
    }

    /* This point should never be reached */
    return 0;
}