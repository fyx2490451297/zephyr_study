#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hello_world, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Hello, World! Zephyr is running.");

    while (1) {
        /* 主线程循环，通过sleep让出CPU时间 */
        k_sleep(K_SECONDS(5));
    }
    return 0;
}
