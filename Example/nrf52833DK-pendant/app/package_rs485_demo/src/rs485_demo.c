#include "rs485.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rs485_demo, LOG_LEVEL_INF);

/* Test frame sent out every CONFIG_PACKAGE_RS485_DEMO_PERIOD_MS. */
static const uint8_t rs485_demo_ping_frame[] = "RS485 PING\r\n";

static void rs485_demo_rx_callback(const uint8_t *data, size_t len, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_HEXDUMP_INF(data, len, "RS485 RX:");
}

static void rs485_demo_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint32_t seq = 0;
	int err;

	err = rs485_register_rx_callback(rs485_demo_rx_callback, NULL);
	if (err != 0) {
		LOG_ERR("Failed to register RS485 RX callback (%d)", err);
	}

	LOG_INF("RS485 demo started, sending a test frame every %d ms",
		CONFIG_PACKAGE_RS485_DEMO_PERIOD_MS);

	while (1) {
		err = rs485_send(rs485_demo_ping_frame, sizeof(rs485_demo_ping_frame) - 1);
		if (err != 0) {
			LOG_WRN("RS485 send failed (%d), seq=%u", err, seq);
		} else {
			LOG_INF("RS485 send ok, seq=%u", seq);
		}

		seq++;
		k_sleep(K_MSEC(CONFIG_PACKAGE_RS485_DEMO_PERIOD_MS));
	}
}

K_THREAD_DEFINE(rs485_demo_tid,
		CONFIG_PACKAGE_RS485_DEMO_STACK_SIZE,
		rs485_demo_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_RS485_DEMO_PRIORITY,
		0, 0);
