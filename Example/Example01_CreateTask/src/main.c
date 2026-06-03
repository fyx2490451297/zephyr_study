#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(thread_demo, LOG_LEVEL_DBG);

/* 1. 线程入口函数（支持接收3个 void* 参数） */
void thread_entry(void *p1, void *p2, void *p3)
{
    int count = 0;
    int param1 = (int)(intptr_t)p1; // 将 void* 转换为 int
    while (1) {
        LOG_INF("Thread is running... count: %d, param1: %d", count++, param1);
        k_sleep(K_SECONDS(1));
    }
}

/* 2. 静态定义并创建线程（编译时搞定，无需再main里写任何代码
 * 参数说明：
    * - thread_id: 线程ID，必须唯一
    * - 1024: 线程栈大小，单位字节
    * - thread_entry: 线程入口函数
    * - (void*)(intptr_t)42: 传递给线程入口函数的
    * 参数1，这里传递了一个整数42，先转换为intptr_t再转换为void*以符合函数参数类型
    * - NULL: 参数2，线程入口函数的第二个参数，这里不使用
    * - NULL: 参数3，线程入口函数的第三个参数，这里不使用
    * - 7: 线程优先级，数值越小优先级
    * - 0: 线程选项，默认即可
    * - 0: 线程延迟启动，0表示立即启动
*/
K_THREAD_DEFINE(thread_id, 1024, thread_entry, (void*)(intptr_t)42, NULL, NULL, 7, 0, 0);

int main(void)
{
    LOG_INF("Main thread is running...");

    /* 主线程循环, thread_id 线程已经在编译时创建并启动 */
    while (1) {
        /* 需要通过sleep来让出CPU时间给其他线程运行，否则主线程会一直占用CPU，导致其他线程无法执行 */
        k_sleep(K_SECONDS(5));
    }
    return 0;
}