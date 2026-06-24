#include "ble_led_ctrl_service.h"
#include "ble_conn.h"
#include "led.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(ble_led_ctrl_svc, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE LED Control Service
 * --------------------------------------------------------------------------
 *
 * This service exposes LED3 and LED4 on the nRF52833DK as two independent
 * GATT characteristics so that a BLE Central (phone app, nRF Connect, etc.)
 * can turn them on and off over the air.
 *
 * Each characteristic supports three ATT operations:
 *
 *   Read   (ATT_READ_REQ)
 *     Central reads the current LED state: 0x00 = OFF, 0x01 = ON.
 *     Useful for the app to sync its UI after connecting.
 *
 *   Write  (ATT_WRITE_REQ)
 *     Central sends a 1-byte value: 0x00 = turn LED OFF, 0x01 = turn ON.
 *     Any other value is rejected with ATT error 0x13 (Value Not Allowed).
 *     After applying the change the service notifies all subscribed
 *     Centrals with the new state (so a second open app sees the change).
 *
 *   Notify (ATT_HANDLE_VALUE_NTF)
 *     Central subscribes by writing 0x0001 to the CCC descriptor.
 *     Notifications are sent:
 *       a) After every successful BLE write (echo back the accepted state).
 *       b) When ble_led_ctrl_service_notify() is called from outside this
 *          module, e.g. by a button handler that also controls the LEDs.
 *
 * GATT attribute table:
 *   [0] Primary Service declaration
 *   [1] LED3 characteristic declaration  (Read + Write + Notify)
 *   [2] LED3 characteristic value        ← read_led3() / write_led3()
 *   [3] LED3 CCC descriptor              ← led3_ccc_changed()
 *   [4] LED4 characteristic declaration  (Read + Write + Notify)
 *   [5] LED4 characteristic value        ← read_led4() / write_led4()
 *   [6] LED4 CCC descriptor              ← led4_ccc_changed()
 *
 * Testing with nRF Connect app:
 *   1. Connect to the device.
 *   2. Find the "Unknown Service" with UUID starting with BBCCDDEE.
 *   3. Write 0x01 to the first characteristic → LED3 lights up.
 *   4. Write 0x00 to the first characteristic → LED3 goes off.
 *   5. Tap the Notify bell icon on each characteristic to subscribe.
 *   6. Write to one LED → the notification echoes the new state instantly.
 * --------------------------------------------------------------------------
 */

/* Attribute indices for bt_gatt_notify(). */
#define LED3_ATTR_IDX  2
#define LED4_ATTR_IDX  5

/* UUID instances. */
static struct bt_uuid_128 led_ctrl_svc_uuid =
	BT_UUID_INIT_128(BLE_LED_CTRL_SVC_UUID_VAL);
static struct bt_uuid_128 led3_uuid =
	BT_UUID_INIT_128(BLE_LED_CTRL_LED3_UUID_VAL);
static struct bt_uuid_128 led4_uuid =
	BT_UUID_INIT_128(BLE_LED_CTRL_LED4_UUID_VAL);

/* --------------------------------------------------------------------------
 * LED state mirror
 * --------------------------------------------------------------------------
 * Tracks the last-applied LED state so Read requests can return it without
 * querying the GPIO driver.
 * --------------------------------------------------------------------------
 */
static uint8_t led3_state; /* 0 = OFF, 1 = ON */
static uint8_t led4_state; /* 0 = OFF, 1 = ON */

/* --------------------------------------------------------------------------
 * CCC subscription state
 * --------------------------------------------------------------------------
 * One atomic per LED so the notify helper can check cheaply without a mutex.
 * --------------------------------------------------------------------------
 */
static atomic_t led3_notify_enabled;
static atomic_t led4_notify_enabled;

static void led3_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	atomic_set(&led3_notify_enabled, enabled ? 1 : 0);
	LOG_INF("LED3 notify %s", enabled ? "enabled" : "disabled");
}

static void led4_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	atomic_set(&led4_notify_enabled, enabled ? 1 : 0);
	LOG_INF("LED4 notify %s", enabled ? "enabled" : "disabled");
}

/* --------------------------------------------------------------------------
 * ATT Read callbacks
 * --------------------------------------------------------------------------
 * Returns the cached LED state (0x00 or 0x01).  The Central can call this
 * immediately after connecting to sync its UI without writing anything.
 * --------------------------------------------------------------------------
 */
static ssize_t read_led3(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &led3_state, sizeof(led3_state));
}

static ssize_t read_led4(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &led4_state, sizeof(led4_state));
}

/* --------------------------------------------------------------------------
 * ATT Write callbacks
 * --------------------------------------------------------------------------
 * Validates and applies the Central's LED command, then pushes a notification
 * so any other subscribed Centrals (or the same app) see the updated state.
 *
 * Protocol (1 byte):
 *   0x00 — turn LED OFF
 *   0x01 — turn LED ON
 *   else — ATT error 0x13 (Value Not Allowed), LED state unchanged
 * --------------------------------------------------------------------------
 */
static ssize_t write_led3(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset,
			  uint8_t flags)
{
	const uint8_t *data = buf;

	if (offset != 0 || len != sizeof(led3_state)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (data[0] > 1U) {
		LOG_WRN("LED3 write: invalid value 0x%02x (expected 0x00 or 0x01)",
			data[0]);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	led3_state = data[0];

	int err = (led3_state == 1U) ? led_on(LED3) : led_off(LED3);

	if (err) {
		LOG_ERR("LED3 gpio %s failed (%d)",
			led3_state ? "on" : "off", err);
	} else {
		LOG_INF("LED3 -> %s (via BLE write)", led3_state ? "ON" : "OFF");
	}

	/* Echo the new state back as a notification so the Central's UI
	 * reflects the applied value immediately, and any other subscriber
	 * is also updated.
	 */
	if (atomic_get(&led3_notify_enabled)) {
		bt_gatt_notify(NULL,
			       &led_ctrl_svc.attrs[LED3_ATTR_IDX],
			       &led3_state, sizeof(led3_state));
	}

	return (ssize_t)len;
}

static ssize_t write_led4(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset,
			  uint8_t flags)
{
	const uint8_t *data = buf;

	if (offset != 0 || len != sizeof(led4_state)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (data[0] > 1U) {
		LOG_WRN("LED4 write: invalid value 0x%02x (expected 0x00 or 0x01)",
			data[0]);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	led4_state = data[0];

	int err = (led4_state == 1U) ? led_on(LED4) : led_off(LED4);

	if (err) {
		LOG_ERR("LED4 gpio %s failed (%d)",
			led4_state ? "on" : "off", err);
	} else {
		LOG_INF("LED4 -> %s (via BLE write)", led4_state ? "ON" : "OFF");
	}

	if (atomic_get(&led4_notify_enabled)) {
		bt_gatt_notify(NULL,
			       &led_ctrl_svc.attrs[LED4_ATTR_IDX],
			       &led4_state, sizeof(led4_state));
	}

	return (ssize_t)len;
}

/* --------------------------------------------------------------------------
 * GATT Service definition
 * --------------------------------------------------------------------------
 */
BT_GATT_SERVICE_DEFINE(led_ctrl_svc,
	/* Primary Service declaration */
	BT_GATT_PRIMARY_SERVICE(&led_ctrl_svc_uuid.uuid),

	/* --- LED3 Control Characteristic -----------------------------------
	 * Properties:
	 *   BT_GATT_CHRC_READ   — Central may read current LED3 state
	 *   BT_GATT_CHRC_WRITE  — Central may write 0x00/0x01 to change state
	 *   BT_GATT_CHRC_NOTIFY — Server pushes state changes to subscribers
	 *
	 * BT_GATT_PERM_WRITE: write permitted without link-layer encryption.
	 * For a production product, use BT_GATT_PERM_WRITE_ENCRYPT to require
	 * an encrypted/bonded connection before allowing LED control.
	 */
	BT_GATT_CHARACTERISTIC(&led3_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_led3, write_led3, NULL),
	BT_GATT_CCC(led3_ccc_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* --- LED4 Control Characteristic ----------------------------------- */
	BT_GATT_CHARACTERISTIC(&led4_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_led4, write_led4, NULL),
	BT_GATT_CCC(led4_ccc_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* --------------------------------------------------------------------------
 * Public notify helper
 * --------------------------------------------------------------------------
 * Called when LED3 or LED4 state changes from a non-BLE source (e.g. a
 * button handler) so subscribed Centrals stay in sync.
 *
 * Pass -1 for either argument to skip that LED.
 * --------------------------------------------------------------------------
 */
void ble_led_ctrl_service_notify(int new_led3_state, int new_led4_state)
{
	struct bt_conn *conn = ble_conn_get_active();

	if (conn == NULL) {
		return;
	}

	if (new_led3_state >= 0) {
		led3_state = (uint8_t)(new_led3_state & 0x01U);
		if (atomic_get(&led3_notify_enabled)) {
			bt_gatt_notify(conn,
				       &led_ctrl_svc.attrs[LED3_ATTR_IDX],
				       &led3_state, sizeof(led3_state));
		}
	}

	if (new_led4_state >= 0) {
		led4_state = (uint8_t)(new_led4_state & 0x01U);
		if (atomic_get(&led4_notify_enabled)) {
			bt_gatt_notify(conn,
				       &led_ctrl_svc.attrs[LED4_ATTR_IDX],
				       &led4_state, sizeof(led4_state));
		}
	}
}

/* --------------------------------------------------------------------------
 * Connection event callback
 * --------------------------------------------------------------------------
 * Reset CCC notify flags on disconnect.  CCC state is per-connection; the
 * next Central must re-subscribe after connecting.
 * --------------------------------------------------------------------------
 */
static void on_conn_event(ble_conn_evt_t evt, struct bt_conn *conn,
			  void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);

	if (evt == BLE_CONN_EVT_DISCONNECTED) {
		atomic_set(&led3_notify_enabled, 0);
		atomic_set(&led4_notify_enabled, 0);
		LOG_DBG("LED ctrl service: CCC state reset on disconnect");
	}
}

/* --------------------------------------------------------------------------
 * Module initialisation
 * --------------------------------------------------------------------------
 */
static int ble_led_ctrl_service_init(void)
{
	int err = ble_conn_register_event_cb(on_conn_event, NULL);

	if (err) {
		LOG_ERR("ble_conn_register_event_cb failed (%d)", err);
		return err;
	}

	/* BT_GATT_SERVICE_DEFINE is static; the service is already registered.
	 * LEDs start in the OFF state (led3_state = led4_state = 0 by default).
	 */
	LOG_INF("LED control service ready — LED3 and LED4 controllable via BLE");
	LOG_INF("  Service UUID: BBCCDDEE-0002-1000-8000-00805F9B34F0");

	return 0;
}

SYS_INIT(ble_led_ctrl_service_init, APPLICATION,
	 CONFIG_PACKAGE_BLE_LED_CTRL_SERVICE_INIT_PRIORITY);
