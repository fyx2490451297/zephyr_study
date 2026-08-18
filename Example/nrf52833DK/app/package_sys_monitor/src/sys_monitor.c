#include "sys_monitor.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(sys_monitor, LOG_LEVEL_INF);

/* --- LVGL FPS tracking --- */

/* Incremented from LVGL's monitor_cb, which fires once per completed screen
 * refresh (i.e. only when some area was actually dirty and got redrawn). */
static volatile uint32_t s_frame_count;

/* LVGL refresh-complete hook: time/px_num are not needed, only the call
 * itself is used as a "one frame done" tick for FPS accounting. */
static void sys_monitor_lv_refr_cb(lv_disp_drv_t *drv, uint32_t time, uint32_t px_num)
{
    ARG_UNUSED(drv);
    ARG_UNUSED(time);
    ARG_UNUSED(px_num);

    s_frame_count++;
}

/* Hooks into the default LVGL display driver's monitor_cb. Returns false
 * while the display hasn't been registered yet (package_lcd_ui creates it
 * from its own thread), so the caller can retry until it succeeds. */
static bool sys_monitor_hook_display(void)
{
    lv_disp_t *disp = lv_disp_get_default();

    if (disp == NULL || disp->driver == NULL) {
        return false;
    }

    disp->driver->monitor_cb = sys_monitor_lv_refr_cb;
    return true;
}

/* --- CPU load --- */

/* Computes the CPU busy percentage since the previous call by diffing
 * cumulative kernel-wide runtime stats (requires CONFIG_SCHED_THREAD_USAGE_ALL).
 * total_cycles excludes the idle thread; execution_cycles is total_cycles +
 * idle_cycles, i.e. the full elapsed window. */
static uint32_t sys_monitor_cpu_load_pct(void)
{
    static k_thread_runtime_stats_t prev;
    k_thread_runtime_stats_t now;
    uint32_t pct = 0;
    int err;

    err = k_thread_runtime_stats_all_get(&now);
    if (err) {
        LOG_WRN("k_thread_runtime_stats_all_get failed (%d)", err);
        return 0;
    }

    uint64_t d_busy = now.total_cycles - prev.total_cycles;
    uint64_t d_total = now.execution_cycles - prev.execution_cycles;

    if (d_total > 0) {
        pct = (uint32_t)((d_busy * 100U) / d_total);
    }

    prev = now;
    return pct;
}

/* --- Periodic report thread --- */

static void sys_monitor_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /* package_lcd_ui creates the LVGL display from its own thread; poll
     * until it is available instead of hard-depending on init order. */
    while (!sys_monitor_hook_display()) {
        k_sleep(K_MSEC(100));
    }

    LOG_INF("System monitor started (period %d ms).", CONFIG_PACKAGE_SYS_MONITOR_PERIOD_MS);

    while (1) {
        k_sleep(K_MSEC(CONFIG_PACKAGE_SYS_MONITOR_PERIOD_MS));

        uint32_t frames = s_frame_count;

        s_frame_count = 0;
        uint32_t fps = (frames * 1000U) / CONFIG_PACKAGE_SYS_MONITOR_PERIOD_MS;

        uint32_t cpu_pct = sys_monitor_cpu_load_pct();

        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        uint32_t ram_used = mon.total_size - mon.free_size;

        LOG_INF("FPS: %u | CPU: %u%% | RAM(LVGL heap): %u%% (%u/%u B)",
                fps, cpu_pct, mon.used_pct, ram_used, mon.total_size);
    }
}

K_THREAD_DEFINE(sys_monitor_tid,
                CONFIG_PACKAGE_SYS_MONITOR_STACK_SIZE,
                sys_monitor_thread,
                NULL, NULL, NULL,
                CONFIG_PACKAGE_SYS_MONITOR_PRIORITY,
                0, 0);
