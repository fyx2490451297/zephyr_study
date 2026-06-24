#ifndef BLE_GATT_SERVICE_H_
#define BLE_GATT_SERVICE_H_

/* --------------------------------------------------------------------------
 * BLE GATT Common Service — Device Information Service (DIS)
 * --------------------------------------------------------------------------
 * UUID 0x180A (Bluetooth SIG assigned)
 *
 * Characteristics:
 *   0x2A29  Manufacturer Name String  — CONFIG_PACKAGE_BLE_GATT_SERVICE_MANUFACTURER
 *   0x2A24  Model Number String       — CONFIG_PACKAGE_BLE_GATT_SERVICE_MODEL
 *   0x2A26  Firmware Revision String  — CONFIG_PACKAGE_BLE_GATT_SERVICE_FW_REV
 *
 * No public API is needed; the service is registered automatically via
 * BT_GATT_SERVICE_DEFINE at link time and becomes active the moment
 * bt_enable() completes in package_ble_core.
 * --------------------------------------------------------------------------
 */

#endif /* BLE_GATT_SERVICE_H_ */
