#include "rs485.h"

#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(rs485_pkg, LOG_LEVEL_INF);

#define UART1_NODE DT_NODELABEL(uart1)

#if !DT_NODE_HAS_STATUS(UART1_NODE, okay)
#error "package_rs485 requires uart1 to be enabled in device tree"
#endif

#define RS485_DE_NODE DT_PATH(zephyr_user)

#if !DT_NODE_HAS_PROP(RS485_DE_NODE, rs485_de_gpios)
#error "package_rs485 requires rs485-de-gpios to be defined under /zephyr,user in device tree"
#endif

static const struct device *const uart_dev = DEVICE_DT_GET(UART1_NODE);
static const struct gpio_dt_spec de_gpio = GPIO_DT_SPEC_GET(RS485_DE_NODE, rs485_de_gpios);

/* --- RX path: UART1 ISR pushes bytes into a ring buffer, a dedicated
 * thread drains it and forwards chunks to the registered callback. --- */
RING_BUF_DECLARE(rx_ring_buf, CONFIG_PACKAGE_RS485_RX_RING_BUFFER_SIZE);
static struct k_spinlock rx_ring_lock;
static struct k_sem rx_data_sem;

static struct k_mutex cb_lock;
static rs485_rx_callback_t rx_callback;
static void *rx_callback_user_data;

/* --- TX path: rs485_send() stages a frame here, the ISR clocks it into
 * the UART FIFO and signals completion once the shift register drains. --- */
static struct k_mutex tx_lock;
static struct k_sem tx_done_sem;
static uint8_t tx_buf[CONFIG_PACKAGE_RS485_TX_BUFFER_SIZE];
static size_t tx_len;
static size_t tx_pos;

static bool rs485_ready;

static void rs485_set_direction_tx(void)
{
	gpio_pin_set_dt(&de_gpio, 1);
}

static void rs485_set_direction_rx(void)
{
	gpio_pin_set_dt(&de_gpio, 0);
}

static void rs485_handle_rx(const struct device *dev)
{
	uint8_t fifo_data[32];

	int received = uart_fifo_read(dev, fifo_data, sizeof(fifo_data));
	if (received <= 0) {
		return;
	}

	k_spinlock_key_t key = k_spin_lock(&rx_ring_lock);
	uint32_t written = ring_buf_put(&rx_ring_buf, fifo_data, (uint32_t)received);
	k_spin_unlock(&rx_ring_lock, key);

	if (written < (uint32_t)received) {
		LOG_WRN("RS485 RX ring buffer overflow, dropped %d bytes",
			(int)received - (int)written);
	}

	k_sem_give(&rx_data_sem);
}

static void rs485_handle_tx(const struct device *dev)
{
	if (tx_pos < tx_len) {
		uint32_t sent = uart_fifo_fill(dev, &tx_buf[tx_pos], tx_len - tx_pos);
		tx_pos += sent;
		return;
	}

	/*
	 * All staged bytes have been handed to the FIFO. Wait for the shift
	 * register to fully drain (uart_irq_tx_complete) before disabling the
	 * TX interrupt and signalling completion, otherwise the transceiver
	 * would be switched back to receive mode while the last byte is
	 * still on the wire.
	 */
	if (uart_irq_tx_complete(dev)) {
		uart_irq_tx_disable(dev);
		k_sem_give(&tx_done_sem);
	}
}

static void rs485_uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			rs485_handle_rx(dev);
		}

		if (uart_irq_tx_ready(dev)) {
			rs485_handle_tx(dev);
		}
	}
}

static void rs485_rx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t chunk[64];

	while (1) {
		k_sem_take(&rx_data_sem, K_FOREVER);

		while (1) {
			uint32_t pulled;
			k_spinlock_key_t key = k_spin_lock(&rx_ring_lock);
			pulled = ring_buf_get(&rx_ring_buf, chunk, sizeof(chunk));
			k_spin_unlock(&rx_ring_lock, key);

			if (pulled == 0U) {
				break;
			}

			k_mutex_lock(&cb_lock, K_FOREVER);
			rs485_rx_callback_t cb = rx_callback;
			void *cb_user_data = rx_callback_user_data;
			k_mutex_unlock(&cb_lock);

			if (cb != NULL) {
				cb(chunk, pulled, cb_user_data);
			}
		}
	}
}

K_THREAD_DEFINE(rs485_rx_tid,
		CONFIG_PACKAGE_RS485_RX_THREAD_STACK_SIZE,
		rs485_rx_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_RS485_RX_THREAD_PRIORITY,
		0, 0);

int rs485_register_rx_callback(rs485_rx_callback_t cb, void *user_data)
{
	if (!rs485_ready) {
		return -EAGAIN;
	}

	k_mutex_lock(&cb_lock, K_FOREVER);
	rx_callback = cb;
	rx_callback_user_data = user_data;
	k_mutex_unlock(&cb_lock);

	return 0;
}

int rs485_send(const uint8_t *data, size_t len)
{
	int err;

	if (!rs485_ready) {
		return -EAGAIN;
	}

	if ((data == NULL) && (len > 0U)) {
		return -EINVAL;
	}

	if (len == 0U) {
		return 0;
	}

	if (len > CONFIG_PACKAGE_RS485_TX_BUFFER_SIZE) {
		return -EMSGSIZE;
	}

	k_mutex_lock(&tx_lock, K_FOREVER);

	memcpy(tx_buf, data, len);
	tx_len = len;
	tx_pos = 0;
	k_sem_reset(&tx_done_sem);

	/*
	 * Drive the transceiver into TX mode before the first bit leaves the
	 * UART, then let the ISR clock bytes into the FIFO until the line
	 * has fully drained.
	 */
	rs485_set_direction_tx();
	uart_irq_tx_enable(uart_dev);

	err = k_sem_take(&tx_done_sem, K_MSEC(CONFIG_PACKAGE_RS485_TX_TIMEOUT_MS));
	if (err != 0) {
		LOG_WRN("RS485 TX did not complete within timeout, forcing RX mode");
		uart_irq_tx_disable(uart_dev);
	}

	/* Always fall back to receive mode so other bus nodes can talk. */
	rs485_set_direction_rx();

	k_mutex_unlock(&tx_lock);

	return err;
}

static int rs485_init(void)
{
	int err;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART1 device not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&de_gpio)) {
		LOG_ERR("RS485 DE/RE GPIO controller not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&de_gpio, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("RS485 DE/RE pin configuration failed (%d)", err);
		return err;
	}

	/* Idle state must be receive mode so the bus is not driven until
	 * rs485_send() is explicitly called.
	 */
	rs485_set_direction_rx();

	k_sem_init(&rx_data_sem, 0, K_SEM_MAX_LIMIT);
	k_sem_init(&tx_done_sem, 0, 1);
	k_mutex_init(&tx_lock);
	k_mutex_init(&cb_lock);

	err = uart_irq_callback_user_data_set(uart_dev, rs485_uart_isr, NULL);
	if (err != 0) {
		LOG_ERR("uart_irq_callback_user_data_set failed (%d)", err);
		return err;
	}

	uart_irq_rx_enable(uart_dev);

	rs485_ready = true;
	LOG_INF("RS485 driver initialized on %s", uart_dev->name);
	return 0;
}

SYS_INIT(rs485_init, APPLICATION, CONFIG_PACKAGE_RS485_INIT_PRIORITY);
