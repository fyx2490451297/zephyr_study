#include "mcu_transport.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(mcu_transport, LOG_LEVEL_INF);

#define UART1_NODE DT_NODELABEL(uart1)

#if !DT_NODE_HAS_STATUS(UART1_NODE, okay)
#error "package_mcu_transport requires uart1 to be enabled in device tree"
#endif

static const struct device *const uart_dev = DEVICE_DT_GET(UART1_NODE);

RING_BUF_DECLARE(rx_ring_buf, CONFIG_PACKAGE_MCU_TRANSPORT_RX_RING_BUFFER_SIZE);
static struct k_spinlock rx_ring_lock;
static struct k_sem rx_data_sem;

static struct k_mutex tx_lock;
static struct k_mutex cb_lock;

static mcu_transport_rx_callback_t rx_callback;
static void *rx_callback_user_data;
static bool transport_ready;

static atomic_t rx_dropped_bytes;

static void mcu_transport_rx_thread(void *p1, void *p2, void *p3)
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
			mcu_transport_rx_callback_t cb = rx_callback;
			void *cb_user_data = rx_callback_user_data;
			k_mutex_unlock(&cb_lock);

			if (cb != NULL) {
				cb(chunk, pulled, cb_user_data);
			}
		}

		atomic_val_t dropped = atomic_set(&rx_dropped_bytes, 0);
		if (dropped > 0) {
			LOG_WRN("RX ring buffer overflow, dropped %d bytes", (int)dropped);
		}
	}
}

K_THREAD_DEFINE(mcu_transport_rx_tid,
		CONFIG_PACKAGE_MCU_TRANSPORT_RX_THREAD_STACK_SIZE,
		mcu_transport_rx_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_MCU_TRANSPORT_RX_THREAD_PRIORITY,
		0, 0);

static void mcu_transport_uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	uint8_t fifo_data[32];

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		int received = uart_fifo_read(dev, fifo_data, sizeof(fifo_data));
		if (received <= 0) {
			continue;
		}

		k_spinlock_key_t key = k_spin_lock(&rx_ring_lock);
		uint32_t written = ring_buf_put(&rx_ring_buf, fifo_data, (uint32_t)received);
		k_spin_unlock(&rx_ring_lock, key);

		if (written < (uint32_t)received) {
			atomic_add(&rx_dropped_bytes, (atomic_val_t)((uint32_t)received - written));
		}

		k_sem_give(&rx_data_sem);
	}
}

int mcu_transport_register_rx_callback(mcu_transport_rx_callback_t cb, void *user_data)
{
	if (!transport_ready) {
		return -EAGAIN;
	}

	k_mutex_lock(&cb_lock, K_FOREVER);
	rx_callback = cb;
	rx_callback_user_data = user_data;
	k_mutex_unlock(&cb_lock);

	return 0;
}

int mcu_transport_send(const uint8_t *data, size_t len)
{
	if (!transport_ready) {
		return -EAGAIN;
	}

	if ((data == NULL) && (len > 0U)) {
		return -EINVAL;
	}

	if (len == 0U) {
		return 0;
	}

	k_mutex_lock(&tx_lock, K_FOREVER);
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart_dev, data[i]);
	}
	k_mutex_unlock(&tx_lock);

	return 0;
}

static int mcu_transport_init(void)
{
	int err;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART1 device not ready");
		return -ENODEV;
	}

	k_sem_init(&rx_data_sem, 0, K_SEM_MAX_LIMIT);
	k_mutex_init(&tx_lock);
	k_mutex_init(&cb_lock);

	err = uart_irq_callback_user_data_set(uart_dev, mcu_transport_uart_isr, NULL);
	if (err != 0) {
		LOG_ERR("uart_irq_callback_user_data_set failed (%d)", err);
		return err;
	}

	uart_irq_rx_enable(uart_dev);

	transport_ready = true;
	LOG_INF("MCU transport initialized on %s", uart_dev->name);
	return 0;
}

SYS_INIT(mcu_transport_init, APPLICATION, CONFIG_PACKAGE_MCU_TRANSPORT_INIT_PRIORITY);
