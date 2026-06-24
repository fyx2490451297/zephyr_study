#include "ble_gatt_service.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_gatt_service, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Device Information Service (DIS) — UUID 0x180A
 * --------------------------------------------------------------------------
 * The DIS is a standard Bluetooth SIG service that allows a Central to read
 * static device metadata without application involvement.
 *
 * All three strings are configured at compile time via Kconfig and exposed
 * as read-only ATT attributes.  No write or notify properties are needed.
 *
 * ATT attribute table layout:
 *   [0] Primary Service declaration (UUID 0x2800, value = 0x180A)
 *   [1] Manufacturer Name declaration (UUID 0x2803)
 *   [2] Manufacturer Name value       (UUID 0x2A29)
 *   [3] Model Number declaration      (UUID 0x2803)
 *   [4] Model Number value            (UUID 0x2A24)
 *   [5] Firmware Revision declaration (UUID 0x2803)
 *   [6] Firmware Revision value       (UUID 0x2A26)
 * --------------------------------------------------------------------------
 */

/* Compile-time string literals sourced from Kconfig. */
static const char manufacturer_str[] = CONFIG_PACKAGE_BLE_GATT_SERVICE_MANUFACTURER;
static const char model_str[]        = CONFIG_PACKAGE_BLE_GATT_SERVICE_MODEL;
static const char fw_rev_str[]       = CONFIG_PACKAGE_BLE_GATT_SERVICE_FW_REV;

/* --------------------------------------------------------------------------
 * ATT read callbacks
 * --------------------------------------------------------------------------
 * bt_gatt_attr_read() copies value[offset .. offset+len] into buf and
 * handles ATT long-read offset arithmetic correctly.
 * --------------------------------------------------------------------------
 */
static ssize_t read_manufacturer(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 manufacturer_str, strlen(manufacturer_str));
}

static ssize_t read_model(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 model_str, strlen(model_str));
}

static ssize_t read_fw_rev(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 fw_rev_str, strlen(fw_rev_str));
}

/* --------------------------------------------------------------------------
 * GATT service registration
 * --------------------------------------------------------------------------
 * BT_GATT_SERVICE_DEFINE() places the attribute table in the
 * "bt_gatt_service_static" iterable section.  Handles are assigned
 * sequentially by the stack during bt_enable().
 * --------------------------------------------------------------------------
 */
BT_GATT_SERVICE_DEFINE(dis_svc,
	/* Primary Service declaration: type = 0x2800, value = UUID 0x180A */
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),

	/* Manufacturer Name String — UUID 0x2A29 */
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_manufacturer, NULL, NULL),

	/* Model Number String — UUID 0x2A24 */
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MODEL_NUMBER,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_model, NULL, NULL),

	/* Firmware Revision String — UUID 0x2A26 */
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_FIRMWARE_REVISION,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_fw_rev, NULL, NULL),
);

static int ble_gatt_service_init(void)
{
	LOG_INF("DIS ready: manufacturer=\"%s\" model=\"%s\" fw_rev=\"%s\"",
		manufacturer_str, model_str, fw_rev_str);
	return 0;
}

SYS_INIT(ble_gatt_service_init, APPLICATION,
	 CONFIG_PACKAGE_BLE_GATT_SERVICE_INIT_PRIORITY);
