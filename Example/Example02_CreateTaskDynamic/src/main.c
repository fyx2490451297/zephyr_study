#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(thread_demo, LOG_LEVEL_DBG);

/* 线程栈大小 */
#define THREAD_STACK_SIZE 1024
/* 线程优先级，数值越小优先级越高 */
#define THREAD_PRIORITY   7

/* 1. 静态分配线程栈内存（编译时预留内存空间，运行时供 k_thread_create 使用） */
K_THREAD_STACK_DEFINE(thread_stack_area, THREAD_STACK_SIZE);

/* 2. 静态分配线程控制块（k_thread 结构体），k_thread_create 需要此结构体来管理线程 */
static struct k_thread thread_data;

/* 3. 线程入口函数（与 K_THREAD_DEFINE 方式相同，支持接收3个 void* 参数） */
void thread_entry(void *p1, void *p2, void *p3)
{
    int count = 0;
    int param1 = (int)(intptr_t)p1; /* 将 void* 转换为 int */
    while (1) {
        LOG_INF("Thread is running... count: %d, param1: %d", count++, param1);
        k_sleep(K_SECONDS(1));
    }
}

int main(void)
{
    LOG_INF("Main thread is running...");

    /*
     * 4. 运行时动态创建线程（区别于 K_THREAD_DEFINE 的编译时静态创建）
     * 参数说明：
     * - &thread_data: 线程控制块指针，之前静态分配的 struct k_thread
     * - thread_stack_area: 线程栈内存，之前通过 K_THREAD_STACK_DEFINE 静态分配
     * - K_THREAD_STACK_SIZEOF(thread_stack_area): 栈的实际大小
     * - thread_entry: 线程入口函数
     * - (void*)(intptr_t)42: 传递给线程入口函数的参数1
     * - NULL: 参数2，线程入口函数的第二个参数，这里不使用
     * - NULL: 参数3，线程入口函数的第三个参数，这里不使用
     * - THREAD_PRIORITY: 线程优先级
     * - 0: 线程选项，默认即可
     * - K_NO_WAIT: 立即启动线程，不延迟
     * 返回值为线程ID (k_tid_t)，可用于后续 k_thread_join/k_thread_abort 等操作
     */
    k_tid_t thread_id = k_thread_create(&thread_data, thread_stack_area,
                                         K_THREAD_STACK_SIZEOF(thread_stack_area),
                                         thread_entry, (void *)(intptr_t)42, NULL, NULL,
                                         THREAD_PRIORITY, 0, K_NO_WAIT);

    /* 可选：给动态创建的线程起一个名字，方便调试时在 shell 中识别 */
    k_thread_name_set(thread_id, "dynamic_thread");

    /* 主线程循环 */
    while (1) {
        /* 需要通过sleep来让出CPU时间给其他线程运行，否则主线程会一直占用CPU，导致其他线程无法执行 */
        k_sleep(K_SECONDS(5));
    }
    return 0;
}
