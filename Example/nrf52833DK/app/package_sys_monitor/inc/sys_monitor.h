#ifndef SYS_MONITOR_H_
#define SYS_MONITOR_H_

/*
 * No public API is exposed: the monitor thread is started automatically via
 * K_THREAD_DEFINE in sys_monitor.c. It periodically logs the LVGL screen
 * FPS, overall CPU load, and LVGL heap RAM usage over the console UART.
 */

#endif /* SYS_MONITOR_H_ */
