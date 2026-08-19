#ifndef LCD_ASYNC_FLUSH_H_
#define LCD_ASYNC_FLUSH_H_

/**
 * @brief Install a non-blocking (EasyDMA async) SPI flush callback on the
 *        default LVGL display driver.
 *
 * Replaces the default flush_cb (which pushes pixels via the blocking
 * mipi_dbi_spi/ILI9341 spi_write() path) with one that drives the SPI bus
 * directly using spi_transceive_cb(). The pixel DMA transfer runs in the
 * background and lv_disp_flush_ready() is signalled from the SPI
 * completion callback, so the calling thread never blocks waiting for the
 * transfer to finish.
 *
 * Must be called after the default LVGL display driver has been
 * registered (i.e. after the display device is confirmed ready) and
 * before the first lv_task_handler() call that triggers a flush.
 */
void lcd_async_flush_install(void);

/**
 * @brief Block the calling thread until any in-flight asynchronous pixel
 *        DMA transfer has completed.
 *
 * The rest of the SPI/MIPI-DBI stack (e.g. display_blanking_off(),
 * ili9xxx command writes) shares the same physical SPI bus as the async
 * flush transfer. Since spi_transceive_cb() returns before the transfer
 * finishes, any other code that issues a blocking SPI transaction right
 * after a flush must call this first, otherwise it contends for the SPI
 * context lock with the still-running DMA transfer instead of running
 * cleanly after it.
 */
void lcd_async_flush_wait_idle(void);

#endif /* LCD_ASYNC_FLUSH_H_ */
