#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_demo, LOG_LEVEL_INF);

/* bt_data is Zephyr's advertising payload format.
 *
 * One bt_data item is one AD structure in the BLE packet:
 *   [Length][Type][Data...]
 *
 * Common fields you may define:
 *   - BT_DATA_FLAGS: discoverable / BR-EDR support flags
 *   - BT_DATA_NAME_COMPLETE or BT_DATA_NAME_SHORTENED: device name
 *   - BT_DATA_UUID16_ALL / BT_DATA_UUID128_ALL: service UUIDs
 *   - BT_DATA_TX_POWER: TX power information
 *   - BT_DATA_MANUFACTURER_DATA: vendor-specific payload
 *   - BT_DATA_SVC_DATA16 / BT_DATA_SVC_DATA128: service data
 *
 * For a beginner demo, flags alone are enough to make the device visible.
 * Later you can add a name or service UUID to help scanners identify it.
 */
static const struct bt_data ad[] = {
	/* Flags tell scanners how this device should be treated.
	 *
	 * BT_LE_AD_GENERAL   -> general discoverable mode
	 * BT_LE_AD_NO_BREDR  -> BLE only, no classic Bluetooth
	 */
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),

	/* If you want to broadcast a custom name in advertising data, you can add:
	 * BT_DATA(BT_DATA_NAME_COMPLETE, "nRF52833 BLE Demo", 17),
	 *
	 * If the packet becomes too large, move the name to scan response data.
	 */
};

/* Scan response data is optional.
 * It is sent only when a scanner actively queries this device.
 * For the current minimal demo, we keep it empty.
 */
static const struct bt_data sd[] = {
};

static int ble_demo_init(void)
{
	int err;

	/* 1) Initialize the Bluetooth subsystem.
	 *    This prepares the controller/host stack before any advertising call.
	 *    Without this step, bt_le_adv_start() will fail.
	 */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (%d)", err);
		return err;
	}

	/* 2) Start connectable advertising.
	 *    BT_LE_ADV_CONN_FAST_1 is a predefined parameter set for a fast,
	 *    connectable advertising interval, suitable for a simple demo.
	 *
	 *    ad[]  -> advertising data (always broadcast)
	 *    sd[]  -> scan response data (sent only after scan request)
	 */
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising start failed (%d)", err);
		return err;
	}

	/* 3) At this point the device is advertising and can be discovered/scanned
	 *    by a central device such as a phone or PC BLE scanner.
	 */
	LOG_INF("BLE advertising started");
	return 0;
}

SYS_INIT(ble_demo_init, APPLICATION, CONFIG_PACKAGE_BLE_DEMO_INIT_PRIORITY);
