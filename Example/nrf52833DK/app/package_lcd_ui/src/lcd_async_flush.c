#include "lcd_async_flush.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(lcd_async_flush, LOG_LEVEL_INF);

/* ILI9341 GRAM addressing/write commands (see display_ili9xxx.h upstream). */
#define ILI9341_CASET_CMD 0x2AU
#define ILI9341_PASET_CMD 0x2BU
#define ILI9341_RAMWR_CMD 0x2CU

static const struct device *const async_flush_mipi_dev =
    DEVICE_DT_GET(DT_NODELABEL(ili9341_mipi_dbi));
static const struct device *const async_flush_spi_dev =
    DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(ili9341_mipi_dbi), spi_dev));
static const struct gpio_dt_spec async_flush_dc_gpio =
    GPIO_DT_SPEC_GET(DT_NODELABEL(ili9341_mipi_dbi), dc_gpios);

/* Reused for both the small blocking command writes (CASET/PASET/RAMWR) and
 * the async pixel payload transfer, so CS/clock/mode stay identical to what
 * the in-tree mipi_dbi_spi driver would have used. */
static const struct mipi_dbi_config async_flush_dbi_config = {
    .mode = MIPI_DBI_MODE_SPI_4WIRE,
    .config = MIPI_DBI_SPI_CONFIG_DT(DT_NODELABEL(ili9341),
                                      SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0),
};

/* Set right before the async transceive is issued, consumed by the SPI
 * completion callback. LVGL never calls flush_cb() again before
 * lv_disp_flush_ready() is signalled, so only one flush is ever in flight. */
static lv_disp_drv_t *async_flush_pending_drv;

static volatile uint32_t async_flush_done_count;

/* Tracks whether a pixel DMA transfer is currently in flight. Taken right
 * before the transfer is kicked off and given back once it completes (or
 * fails to start), so lcd_async_flush_wait_idle() can block callers that
 * need the SPI bus to themselves (e.g. display_blanking_off()) until the
 * transfer is truly done, instead of letting them race for the SPI
 * context lock against the still-running DMA transfer. */
static K_SEM_DEFINE(async_flush_idle_sem, 1, 1);

static void lcd_async_flush_done(const struct device *dev, int result, void *userdata)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(userdata);

    async_flush_done_count++;
    if (result < 0) {
        LOG_ERR("Async pixel DMA transfer failed (%d)", result);
    }

    k_sem_give(&async_flush_idle_sem);

    /* Runs in SPI driver completion context (ISR or work queue);
     * lv_disp_flush_ready() is documented as interrupt-safe. */
    lv_disp_flush_ready(async_flush_pending_drv);
}

void lcd_async_flush_wait_idle(void)
{
    k_sem_take(&async_flush_idle_sem, K_FOREVER);
    k_sem_give(&async_flush_idle_sem);
}

static void lcd_async_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                                lv_color_t *color_p)
{
    uint16_t x = area->x1;
    uint16_t y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1U;
    uint16_t h = area->y2 - area->y1 + 1U;
    uint8_t caset[4];
    uint8_t paset[4];
    struct spi_buf tx_buf;
    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1,
    };
    int err;
    static uint32_t call_count;

    call_count++;
    if (call_count <= 3) {
        LOG_INF("flush #%u area (%u,%u)-(%u,%u) w=%u h=%u buf=%p",
                 call_count, x, y, area->x2, area->y2, w, h, (void *)color_p);
    }

    sys_put_be16(x, &caset[0]);
    sys_put_be16(x + w - 1U, &caset[2]);
    sys_put_be16(y, &paset[0]);
    sys_put_be16(y + h - 1U, &paset[2]);

    /* Column/page address window + RAM-write command are tiny (4-6 byte)
     * transfers; sending them blocking keeps this code simple and costs
     * only a few microseconds. Only the pixel payload below -- the part
     * that actually takes meaningful DMA time -- is asynchronous. */
    err = mipi_dbi_command_write(async_flush_mipi_dev, &async_flush_dbi_config,
                                  ILI9341_CASET_CMD, caset, sizeof(caset));
    if (err) {
        LOG_ERR("CASET write failed (%d)", err);
        goto fail;
    }

    err = mipi_dbi_command_write(async_flush_mipi_dev, &async_flush_dbi_config,
                                  ILI9341_PASET_CMD, paset, sizeof(paset));
    if (err) {
        LOG_ERR("PASET write failed (%d)", err);
        goto fail;
    }

    err = mipi_dbi_command_write(async_flush_mipi_dev, &async_flush_dbi_config,
                                  ILI9341_RAMWR_CMD, NULL, 0);
    if (err) {
        LOG_ERR("RAMWR write failed (%d)", err);
        goto fail;
    }

    tx_buf.buf = color_p;
    tx_buf.len = (size_t)w * h * sizeof(lv_color_t);

    async_flush_pending_drv = disp_drv;

    /* Mark the bus busy before the transfer is handed off, so
     * lcd_async_flush_wait_idle() blocks any caller that needs the SPI
     * bus (e.g. display_blanking_off()) until lcd_async_flush_done()
     * releases it. LVGL guarantees only one flush is ever in flight, so
     * this take() can never contend with itself. */
    k_sem_take(&async_flush_idle_sem, K_FOREVER);

    /* Switch DC high for data, then hand the pixel buffer straight to the
     * nRF52833 SPIM/EasyDMA peripheral without waiting for completion. */
    gpio_pin_set_dt(&async_flush_dc_gpio, 1);
    err = spi_transceive_cb(async_flush_spi_dev, &async_flush_dbi_config.config,
                             &tx_bufs, NULL, lcd_async_flush_done, NULL);
    if (call_count <= 3) {
        LOG_INF("flush #%u spi_transceive_cb ret=%d", call_count, err);
    }
    if (err) {
        LOG_ERR("Async SPI transceive failed (%d)", err);
        /* Transfer never started, so lcd_async_flush_done() will not run
         * to release the semaphore -- release it here instead. */
        k_sem_give(&async_flush_idle_sem);
        goto fail;
    }

    return;

fail:
    /* Fall back to signalling flush-ready immediately so LVGL does not
     * stall forever on a transfer that never started. */
    lv_disp_flush_ready(disp_drv);
}

void lcd_async_flush_install(void)
{
    lv_disp_t *disp = lv_disp_get_default();

    if (!disp || !disp->driver) {
        LOG_ERR("No default LVGL display driver registered yet");
        return;
    }

    if (!device_is_ready(async_flush_spi_dev)) {
        LOG_ERR("SPI device %s not ready", async_flush_spi_dev->name);
        return;
    }

    if (!device_is_ready(async_flush_mipi_dev)) {
        LOG_ERR("MIPI-DBI device %s not ready", async_flush_mipi_dev->name);
        return;
    }

    LOG_INF("async spi_dev=%s mipi_dev=%s dc_gpio=%s pin=%u",
            async_flush_spi_dev->name, async_flush_mipi_dev->name,
            async_flush_dc_gpio.port->name, async_flush_dc_gpio.pin);

    disp->driver->flush_cb = lcd_async_flush_cb;
    LOG_INF("Installed non-blocking (EasyDMA async) SPI flush callback");
}
