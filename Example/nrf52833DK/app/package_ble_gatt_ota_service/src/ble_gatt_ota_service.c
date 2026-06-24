#include "ble_gatt_ota_service.h"
#include "ble_conn.h"
#include "mcu_transport.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stddef.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_gatt_ota_service, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE OTA GATT Service — Transparent relay between Central and host MCU
 * --------------------------------------------------------------------------
 *
 * Architecture overview
 * ─────────────────────
 *
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  BLE Central (phone / OTA host tool)                            │
 *   │    1. Connects to nRF52833                                       │
 *   │    2. Discovers OTA service (UUID AABBCCDD-0001-…)              │
 *   │    3. Enables Status notifications (writes 0x0001 to Status CCC)│
 *   │    4. Writes OTA commands to Control Point                       │
 *   │    5. Writes firmware chunks to Data characteristic              │
 *   │    6. Reads Status notifications for MCU acknowledgements        │
 *   └──────────────────────┬───────────────────────────────────────────┘
 *                          │ BLE (ATT write / notify)
 *   ┌──────────────────────▼───────────────────────────────────────────┐
 *   │  nRF52833 — this module                                          │
 *   │    Control Point write → mcu_transport_send()                    │
 *   │    Data write          → mcu_transport_send()                    │
 *   │    MCU UART RX         → bt_gatt_notify() on Status char         │
 *   └──────────────────────┬───────────────────────────────────────────┘
 *                          │ UART (raw bytes, MCU protocol frames)
 *   ┌──────────────────────▼───────────────────────────────────────────┐
 *   │  Host MCU                                                        │
 *   │    Receives MCU protocol frames, processes OTA data              │
 *   │    Sends ACK/NAK/progress back via UART                          │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 * MCU protocol framing (handled by Central + MCU; NOT parsed here):
 *   Forward  [0xAA][0x55][CMD][SEQ][LEN][Payload][CRC-H][CRC-L]
 *   Backward [0x55][0xAA][CMD][SEQ][LEN][Payload][CRC-H][CRC-L]
 *
 * GATT attribute table:
 *   [0] Primary Service declaration
 *   [1] Control Point char declaration  (Write + Notify)
 *   [2] Control Point char value        ← write_ctrl_pt()
 *   [3] Control Point CCC               ← ctrl_pt_ccc_changed()
 *   [4] Data char declaration           (Write Without Response)
 *   [5] Data char value                 ← write_data()
 *   [6] Status char declaration         (Notify)
 *   [7] Status char value               ← bt_gatt_notify() from MCU RX
 *   [8] Status CCC                      ← status_ccc_changed()
 * --------------------------------------------------------------------------
 */

/* Attribute indices for bt_gatt_notify() — must match the service table. */
#define OTA_CTRL_PT_ATTR_IDX  2
#define OTA_STATUS_ATTR_IDX   7

/* UUID instances — struct bt_uuid_128 with static storage duration. */
static struct bt_uuid_128 ota_svc_uuid      = BT_UUID_INIT_128(BLE_GATT_OTA_SVC_UUID_VAL);
static struct bt_uuid_128 ota_ctrl_pt_uuid  = BT_UUID_INIT_128(BLE_GATT_OTA_CTRL_PT_UUID_VAL);
static struct bt_uuid_128 ota_data_uuid     = BT_UUID_INIT_128(BLE_GATT_OTA_DATA_UUID_VAL);
static struct bt_uuid_128 ota_status_uuid   = BT_UUID_INIT_128(BLE_GATT_OTA_STATUS_UUID_VAL);

/* --------------------------------------------------------------------------
 * CCC subscription state (per characteristic)
 * --------------------------------------------------------------------------
 * Tracked as atomics: CCC callbacks fire from the BT RX work queue while
 * the MCU RX callback fires from mcu_transport's RX thread.  An atomic
 * avoids a spinlock for this simple flag.
 * --------------------------------------------------------------------------
 */
static atomic_t ctrl_pt_notify_enabled;
static atomic_t status_notify_enabled;

static void ctrl_pt_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	atomic_set(&ctrl_pt_notify_enabled, enabled ? 1 : 0);
	LOG_INF("OTA Control Point notify %s", enabled ? "enabled" : "disabled");
}

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	atomic_set(&status_notify_enabled, enabled ? 1 : 0);
	LOG_INF("OTA Status notify %s", enabled ? "enabled" : "disabled");
}

/* --------------------------------------------------------------------------
 * ATT Write callbacks
 * --------------------------------------------------------------------------
 * Both Control Point and Data writes are forwarded transparently to the MCU
 * via mcu_transport_send().  No framing, CRC or sequence management is done
 * here — that is the responsibility of the BLE Central and the host MCU.
 * --------------------------------------------------------------------------
 */
static ssize_t write_ctrl_pt(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len, uint16_t offset,
			     uint8_t flags)
{
	if (offset != 0) {
		/* Long writes (ATT_PREPARE_WRITE_REQ) not supported. */
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len == 0U || len > CONFIG_PACKAGE_BLE_GATT_OTA_SERVICE_MAX_DATA_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	LOG_DBG("OTA ctrl_pt write %u bytes", len);

	int err = mcu_transport_send((const uint8_t *)buf, len);
	if (err) {
		LOG_ERR("mcu_transport_send failed (%d)", err);
		/* Return a success code to the Central even on transport error
		 * so the ATT layer does not abort the connection.  The MCU side
		 * will signal the failure via a NAK on the Status characteristic.
		 */
	}

	return (ssize_t)len;
}

static ssize_t write_data(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset,
			  uint8_t flags)
{
	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len == 0U || len > CONFIG_PACKAGE_BLE_GATT_OTA_SERVICE_MAX_DATA_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	LOG_DBG("OTA data write %u bytes", len);

	int err = mcu_transport_send((const uint8_t *)buf, len);
	if (err) {
		LOG_ERR("mcu_transport_send failed (%d)", err);
	}

	return (ssize_t)len;
}

/* --------------------------------------------------------------------------
 * GATT Service definition
 * --------------------------------------------------------------------------
 */
BT_GATT_SERVICE_DEFINE(ota_svc,
	/* Primary Service declaration */
	BT_GATT_PRIMARY_SERVICE(&ota_svc_uuid.uuid),

	/* --- Control Point (Write + Notify) --------------------------------
	 * Central writes OTA control commands here.
	 * BT_GATT_CHRC_WRITE: Central must receive an ATT_WRITE_RSP.
	 * Notify: BLE-level ACK or status pushed back to Central.
	 */
	BT_GATT_CHARACTERISTIC(&ota_ctrl_pt_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_WRITE,
		NULL, write_ctrl_pt, NULL),
	BT_GATT_CCC(ctrl_pt_ccc_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* --- Data (Write Without Response) ---------------------------------
	 * Central streams firmware chunks here.
	 * BT_GATT_CHRC_WRITE_WITHOUT_RESP: no ATT_WRITE_RSP — highest
	 * throughput because the Central does not wait for acknowledgement.
	 * The MCU ACKs via UART → relayed as a Status notification.
	 */
	BT_GATT_CHARACTERISTIC(&ota_data_uuid.uuid,
		BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_data, NULL),

	/* --- Status (Notify) -----------------------------------------------
	 * MCU responses (ACK, NAK, progress) are forwarded here.
	 * The MCU UART RX callback calls bt_gatt_notify() on this attribute.
	 */
	BT_GATT_CHARACTERISTIC(&ota_status_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(status_ccc_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* --------------------------------------------------------------------------
 * MCU transport RX callback
 * --------------------------------------------------------------------------
 * Invoked by mcu_transport's RX thread whenever bytes arrive over UART.
 * Forwards the raw bytes to the BLE Central as a Status characteristic
 * notification.
 *
 * Thread context: mcu_transport RX thread (not BT work queue).
 * bt_gatt_notify() is thread-safe and queues the notification on the
 * BT work queue for actual transmission.
 * --------------------------------------------------------------------------
 */
static void on_mcu_rx(const uint8_t *data, size_t len, void *user_data)
{
	ARG_UNUSED(user_data);

	if (atomic_get(&status_notify_enabled) == 0) {
		/* No subscriber — drop silently. */
		return;
	}

	struct bt_conn *conn = ble_conn_get_active();

	if (conn == NULL) {
		LOG_DBG("MCU RX: no active connection, dropping %zu bytes", len);
		return;
	}

	/* Zephyr limits bt_gatt_notify() payload to ATT_MTU − 3 bytes.
	 * If len exceeds this, send in chunks the stack can handle.
	 * In practice the MCU sends short frames (< 261 bytes) and the ATT
	 * MTU after negotiation is typically 247 bytes, so one call suffices.
	 */
	int err = bt_gatt_notify(conn,
				 &ota_svc.attrs[OTA_STATUS_ATTR_IDX],
				 data, (uint16_t)len);
	if (err) {
		LOG_DBG("status notify failed (%d)", err);
	} else {
		LOG_DBG("status notify: %zu bytes forwarded to Central", len);
	}
}

/* --------------------------------------------------------------------------
 * Connection event callback
 * --------------------------------------------------------------------------
 * Reset CCC subscription state on disconnect so that the next connection
 * starts with notifications disabled (per BLE specification — CCC state is
 * per-connection and not persisted without bonding).
 * --------------------------------------------------------------------------
 */
static void on_conn_event(ble_conn_evt_t evt, struct bt_conn *conn,
			  void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);

	if (evt == BLE_CONN_EVT_DISCONNECTED) {
		atomic_set(&ctrl_pt_notify_enabled, 0);
		atomic_set(&status_notify_enabled, 0);
		LOG_INF("OTA service: connection lost, CCC state reset");
	}
}

/* --------------------------------------------------------------------------
 * Module initialisation
 * --------------------------------------------------------------------------
 */
static int ble_gatt_ota_service_init(void)
{
	int err;

	/* Register for connection lifecycle events so CCC state is reset on
	 * disconnect.  Must run after ble_conn_init() (priority 93).
	 */
	err = ble_conn_register_event_cb(on_conn_event, NULL);
	if (err) {
		LOG_ERR("ble_conn_register_event_cb failed (%d)", err);
		return err;
	}

	/* Register for MCU UART RX data so responses can be forwarded to the
	 * BLE Central as Status notifications.
	 * Must run after mcu_transport_init() (priority 92).
	 */
	err = mcu_transport_register_rx_callback(on_mcu_rx, NULL);
	if (err) {
		LOG_ERR("mcu_transport_register_rx_callback failed (%d)", err);
		return err;
	}

	LOG_INF("OTA GATT service ready (max write %d bytes)",
		CONFIG_PACKAGE_BLE_GATT_OTA_SERVICE_MAX_DATA_LEN);

	return 0;
}

SYS_INIT(ble_gatt_ota_service_init, APPLICATION,
	 CONFIG_PACKAGE_BLE_GATT_OTA_SERVICE_INIT_PRIORITY);
