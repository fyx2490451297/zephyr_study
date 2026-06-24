#include "ble_core.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_core, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE Core — Single bt_enable() owner
 * --------------------------------------------------------------------------
 * The Zephyr Bluetooth stack must be initialised exactly once per firmware
 * image.  This module owns that call and exposes ble_core_is_ready() so
 * other packages can verify the stack is up before issuing BT API calls.
 *
 * All other BLE packages (ble_adv, ble_conn, gatt services, gatt client)
 * must declare a higher SYS_INIT priority number than this module so that
 * the stack is guaranteed to be initialised before they run.
 *
 * Boot sequence (APPLICATION level, in priority order):
 *   ble_core_init()          priority 90  — bt_enable()
 *   ble_adv_init()           priority 91  — start advertising
 *   mcu_transport_init()     priority 92  — UART transport
 *   ble_conn_init()          priority 93  — register conn callbacks
 *   ble_gatt_service_init()  priority 94  — log DIS ready
 *   ble_gatt_ota_service_init() priority 95 — register MCU RX callback
 *   ble_gatt_client_init()   priority 96  — GATT client setup
 * --------------------------------------------------------------------------
 */

static bool bt_ready;

bool ble_core_is_ready(void)
{
	return bt_ready;
}

static int ble_core_init(void)
{
	int err;

	/* bt_enable(NULL) — synchronous init: blocks until the BLE Host and
	 * Controller are fully operational.  Passing a callback pointer would
	 * make it asynchronous; synchronous is simpler and correct here because
	 * SYS_INIT callbacks run sequentially before the scheduler starts.
	 */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}

	bt_ready = true;
	LOG_INF("Bluetooth stack initialised");

	return 0;
}

SYS_INIT(ble_core_init, APPLICATION, CONFIG_PACKAGE_BLE_CORE_INIT_PRIORITY);
