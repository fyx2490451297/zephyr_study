#include "pipe_bender_ui.h"
#include "lcd_backlight.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(lcd_ui, LOG_LEVEL_INF);

static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* Memory Access Control (MADCTL, cmd 0x36) register and the value this panel
 * actually needs.
 *
 * The in-tree ILI9341 driver derives MADCTL purely from the DT "rotation"
 * property and, for a 90 degree rotation, always writes MV|BGR (0x28). On
 * this panel that produces a left-right (horizontal) mirrored image because
 * of how its GRAM is wired, and the driver offers no devicetree knob to
 * override MADCTL directly. Re-issuing MADCTL here with the MX bit added
 * (0x68 = MV|MX|BGR) keeps the same landscape rotation while correcting the
 * mirroring, without patching the shared Zephyr driver.
 *
 * The panel is further rotated 180 degrees clockwise on top of that fix by
 * toggling both MX and MY (row/column address order): MV|MY|BGR = 0xA8.
 */
#define ILI9341_MADCTL_CMD   0x36U
#define ILI9341_MADCTL_FIXED 0x68U

static void lcd_ui_fix_mirrored_madctl(void)
{
    static const struct device *const mipi_dev =
        DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(ili9341)));
    static const struct mipi_dbi_config dbi_config = {
        .mode = MIPI_DBI_MODE_SPI_4WIRE,
        .config = MIPI_DBI_SPI_CONFIG_DT(DT_NODELABEL(ili9341),
                                          SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0),
    };
    uint8_t madctl = ILI9341_MADCTL_FIXED;
    int err;

    if (!device_is_ready(mipi_dev)) {
        LOG_ERR("MIPI-DBI device %s not ready!", mipi_dev->name);
        return;
    }

    err = mipi_dbi_command_write(mipi_dev, &dbi_config, ILI9341_MADCTL_CMD, &madctl, 1U);
    if (err) {
        LOG_ERR("Failed to correct MADCTL (%d)", err);
    }
}

static void lcd_ui_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device %s not ready!", display_dev->name);
        return;
    }

    /* Correct the panel's horizontal mirroring before the first frame is
     * drawn, so nothing gets rendered with the wrong MADCTL setting. */
    lcd_ui_fix_mirrored_madctl();

    pipe_bender_ui_create();

    /* Render the first frame before turning the backlight/blanking on,
     * so the panel doesn't flash stale/garbage content. */
    lv_task_handler();
    display_blanking_off(display_dev);
    lcd_backlight_on();

    LOG_INF("LCD UI thread started.");

    while (1) {
        lv_task_handler();
        k_sleep(K_MSEC(10));
    }
}

K_THREAD_DEFINE(lcd_ui_tid,
                CONFIG_PACKAGE_LCD_UI_STACK_SIZE,
                lcd_ui_thread,
                NULL, NULL, NULL,
                CONFIG_PACKAGE_LCD_UI_PRIORITY,
                0, 0);
