#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* 直接引用 package_led 暴露出的头文件 */
#include "app_led.h"

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("========== STM32F407 Minimal System ==========");
    LOG_INF("系统启动，进入主业务循环！");
    LOG_INF("注意：LED 硬件初始化已被 SYS_INIT 自动接管！");

    while (1) {
        /* 因为在 prj.conf 里开启了 AUTO_BLINK，
         * 这里什么都不用干，LED 就会自己在后台闪。
         * 如果关闭了 AUTO_BLINK，你可以取消下面这行的注释来手动控制：
         */
        // app_led_toggle();

        LOG_INF("主线程心跳运行中...");
        k_msleep(5000);
    }
    return 0;
}