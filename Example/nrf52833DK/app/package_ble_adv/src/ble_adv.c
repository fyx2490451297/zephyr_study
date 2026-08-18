#include "ble_adv.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(ble_adv, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE Advertising Manager
 * --------------------------------------------------------------------------
 * This module centralises all advertising state.  It is the single point
 * that calls bt_le_adv_start() and bt_le_adv_stop(), which prevents races
 * when multiple modules (conn manager, OTA service) want to control
 * advertising.
 *
 * Advertising data layout:
 *
 *   Primary AD packet (always broadcast, 31-byte budget):
 *     BT_DATA_FLAGS            — 3 bytes
 *     BT_DATA_NAME_COMPLETE    — 2 + len(name) bytes
 *
 *   Scan Response (returned after active Scan Request):
 *     empty — name already fits in the primary packet for typical names
 *
 * The BLE stack stops advertising automatically once a Central establishes
 * a connection.  package_ble_conn calls ble_adv_start() after every
 * disconnection to re-enter the advertising state.
 * --------------------------------------------------------------------------
 */

static const struct bt_data ad[] = {
	/* Flags: general discoverable, BLE-only (no BR/EDR). */
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),

	/* Complete Local Name — makes the device easily identifiable in
	 * scanner apps (nRF Connect, LightBlue, etc.) without requiring a
	 * connection or service discovery.
	 */
	BT_DATA(BT_DATA_NAME_COMPLETE,
		CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static atomic_t adv_active;

bool ble_adv_is_active(void)
{
	return (atomic_get(&adv_active) != 0);
}

int ble_adv_start(void)
{
	int err;

	/* bt_le_adv_start() arguments:
	 *   Connectable advertising, interval 30-60 ms (BT_GAP_ADV_FAST_INT_MIN_1
	 *   / MAX_1). Shorter interval means faster discovery but higher radio
	 *   duty cycle and power use.
	 *   ad / ARRAY_SIZE(ad)   — primary advertising payload
	 *   NULL / 0              — no scan response data
	 */
	err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE,
					      BT_GAP_ADV_FAST_INT_MIN_1,
					      BT_GAP_ADV_FAST_INT_MAX_1, NULL),
			      ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		if (err == -EALREADY) {
			LOG_WRN("advertising already active");
		} else {
			LOG_ERR("bt_le_adv_start failed (%d)", err);
		}
		return err;
	}

	atomic_set(&adv_active, 1);
	LOG_INF("advertising started");

	return 0;
}

int ble_adv_stop(void)
{
	int err;

	err = bt_le_adv_stop();
	if (err) {
		LOG_ERR("bt_le_adv_stop failed (%d)", err);
		return err;
	}

	atomic_set(&adv_active, 0);
	LOG_INF("advertising stopped");

	return 0;
}

static int ble_adv_init(void)
{
	int err;

	/* Start advertising immediately at boot so the device is discoverable
	 * as soon as the BLE stack (ble_core, priority 90) is ready.
	 */
	err = ble_adv_start();
	if (err) {
		LOG_ERR("initial advertising start failed (%d)", err);
		return err;
	}

	return 0;
}

SYS_INIT(ble_adv_init, APPLICATION, CONFIG_PACKAGE_BLE_ADV_INIT_PRIORITY);
