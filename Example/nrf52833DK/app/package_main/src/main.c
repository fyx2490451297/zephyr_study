#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_DBG);

int main(void)
{
    LOG_INF("Hello, Zephyr on nRF52833DK!");

    /* Hardware (LED, KEY) is auto-initialized by SYS_INIT in their respective packages.
     * Application threads (led_ctrl) are started automatically via K_THREAD_DEFINE.
     * main() only handles top-level application logic. */

    while (1) {
        LOG_DBG("Main loop is running...");
        k_sleep(K_SECONDS(2));
    }

    return 0;
}