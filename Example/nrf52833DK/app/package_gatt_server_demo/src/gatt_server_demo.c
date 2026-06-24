/* No public API for this demo module. */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(gatt_server_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * GATT Server Demo — Learning Notes
 * --------------------------------------------------------------------------
 *
 * ATT (Attribute Protocol)
 * ────────────────────────
 * ATT is the foundation of all BLE data exchange. It defines a client/server
 * model where every piece of information on the server lives in an attribute:
 *
 *   ┌──────────────┬──────────────────┬─────────────┬───────────────────┐
 *   │ Handle (2 B) │ Type (UUID)      │ Permissions │ Value (0–512 B)   │
 *   └──────────────┴──────────────────┴─────────────┴───────────────────┘
 *
 *   Handle      — A 16-bit number auto-assigned by the stack, used to
 *                 address a specific attribute in ATT_READ_REQ /
 *                 ATT_WRITE_REQ PDUs.
 *   Type (UUID) — 2-byte Bluetooth SIG-assigned or 16-byte custom UUID
 *                 that declares what kind of data the attribute holds.
 *   Permissions — Read, Write, Encrypt required, Authenticate required, etc.
 *   Value       — The actual payload; up to 512 bytes for ATT long reads.
 *
 * GATT (Generic Attribute Profile)
 * ─────────────────────────────────
 * GATT adds a hierarchical data model on top of the flat ATT attribute table.
 * Every node in the hierarchy is represented by one or more ATT attributes:
 *
 *   Profile
 *   └── Service  ← "Primary Service" attribute (UUID 0x2800)
 *                   Value = service UUID; marks where a service starts.
 *       └── Characteristic  ← two ATT attributes per characteristic:
 *           ├── Declaration  (UUID 0x2803)
 *           │     Value = [Properties (1 B)][Value Handle (2 B)][Char UUID]
 *           │     Tells the Central what operations are allowed and
 *           │     which handle holds the actual data.
 *           ├── Value        (UUID = the characteristic's own UUID)
 *           │     Value = the application data (firmware version, sensor
 *           │     reading, control register, etc.)
 *           └── Descriptor(s)  (optional)
 *               e.g. CCC (0x2902), CPFD (0x2904), User Description (0x2901)
 *
 * Characteristic Properties (1-byte bitmask stored in the Declaration):
 *   0x02  Read                   — Central may issue ATT_READ_REQ
 *   0x04  Write Without Response — Central may write without ATT reply
 *   0x08  Write                  — Central may issue ATT_WRITE_REQ
 *   0x10  Notify                 — Server may push ATT_HANDLE_VALUE_NTF
 *   0x20  Indicate               — Same as Notify but Central sends ACK
 *
 * CCC Descriptor (UUID 0x2902 — Client Characteristic Configuration)
 * ──────────────────────────────────────────────────────────────────
 * A 2-byte descriptor written by the Central to opt in / out of
 * server-initiated updates.  Must be present for every Notify/Indicate char.
 *
 *   0x0000 — disabled  (default after connection)
 *   0x0001 — notifications enabled
 *   0x0002 — indications enabled
 *
 * ATT PDU flow for a Read (Central reads a characteristic value):
 *
 *   Central  →  ATT_READ_REQ   (handle)
 *   Server   →  ATT_READ_RSP   (value)
 *
 * ATT PDU flow for a Write (Central writes a characteristic value):
 *
 *   Central  →  ATT_WRITE_REQ  (handle, value)
 *   Server   →  ATT_WRITE_RSP           — on success
 *            or ATT_ERROR_RSP  (errcode) — on failure
 *
 * ATT PDU flow for a Notification (Server pushes to Central):
 *
 *   Server   →  ATT_HANDLE_VALUE_NTF (handle, value)
 *   [Central does NOT send an acknowledgement for notifications]
 *
 * This Demo's Service Layout
 * ──────────────────────────
 *   Custom Service  UUID: 12345678-1234-5678-1234-56789abcdef0
 *
 *   ┌──────────────────┬─────────────────────┬─────────────────────────────┐
 *   │ Characteristic   │ Properties          │ Description                 │
 *   ├──────────────────┼─────────────────────┼─────────────────────────────┤
 *   │ Firmware Version │ Read                │ Const string "1.0.0-demo"   │
 *   │ LED Control      │ Read / Write        │ 1 byte: 0x00=OFF, 0x01=ON   │
 *   │ Counter          │ Read / Notify + CCC │ uint16_t, +1 every second   │
 *   └──────────────────┴─────────────────────┴─────────────────────────────┘
 *
 * Attribute table index map (used when calling bt_gatt_notify):
 *   [0] Primary Service declaration
 *   [1] Firmware Version char declaration
 *   [2] Firmware Version char value       ← read_fw_ver()
 *   [3] LED Control char declaration
 *   [4] LED Control char value            ← read_led_ctrl() / write_led_ctrl()
 *   [5] Counter char declaration
 *   [6] Counter char value                ← read_counter() / bt_gatt_notify()
 *   [7] Counter CCC descriptor            ← counter_ccc_changed()
 *
 * How to test with the "nRF Connect" mobile / desktop app:
 *   1. Scan and connect to "nRF52833 BLE Demo".
 *   2. Navigate to the Unknown Service (custom UUID).
 *   3. Tap the Read icon on Firmware Version — expect "1.0.0-demo".
 *   4. Tap the Read icon on LED Control — expect 0x00.
 *   5. Write 0x01 to LED Control — watch the LOG_INF output on the device.
 *   6. Tap the Notify (bell) icon on Counter — values increment every second.
 * --------------------------------------------------------------------------
 */

/* --------------------------------------------------------------------------
 * Custom UUID definitions
 * --------------------------------------------------------------------------
 * 128-bit UUIDs are used for proprietary (non-SIG) services and
 * characteristics.  16-bit UUIDs are reserved for Bluetooth SIG-assigned
 * profiles (e.g. 0x180D = Heart Rate Service, 0x180A = Device Information).
 *
 * BT_UUID_128_ENCODE() serialises the UUID fields into a byte sequence in
 * the little-endian on-wire BLE format required by BT_UUID_INIT_128.
 * --------------------------------------------------------------------------
 */
#define GATT_DEMO_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define GATT_DEMO_FW_VER_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define GATT_DEMO_LED_CTRL_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)
#define GATT_DEMO_COUNTER_UUID_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)

static struct bt_uuid_128 gatt_demo_svc_uuid     = BT_UUID_INIT_128(GATT_DEMO_SVC_UUID_VAL);
static struct bt_uuid_128 gatt_demo_fw_ver_uuid  = BT_UUID_INIT_128(GATT_DEMO_FW_VER_UUID_VAL);
static struct bt_uuid_128 gatt_demo_led_ctrl_uuid = BT_UUID_INIT_128(GATT_DEMO_LED_CTRL_UUID_VAL);
static struct bt_uuid_128 gatt_demo_counter_uuid  = BT_UUID_INIT_128(GATT_DEMO_COUNTER_UUID_VAL);

/* --------------------------------------------------------------------------
 * Module-private state
 * --------------------------------------------------------------------------
 */

/* Firmware version string exposed as a read-only GATT characteristic.
 * The null terminator is excluded when sending over ATT.
 */
static const char fw_ver_str[] = "1.0.0-demo";

/* LED control register: 0x00 = OFF, 0x01 = ON.  Written by the Central. */
static uint8_t led_ctrl_val;

/* Counter value auto-incremented by the notification thread. */
static uint16_t counter_val;

/* Non-zero when the Central has written 0x0001 to the Counter CCC descriptor,
 * enabling notifications.  Accessed from both the BT RX work queue and the
 * notification thread, so it must be an atomic.
 */
static atomic_t notify_enabled;

/* Active connection handle; NULL when the device is not connected. */
static struct bt_conn *current_conn;

/* --------------------------------------------------------------------------
 * ATT Read callbacks
 * --------------------------------------------------------------------------
 * The GATT server calls a read callback whenever a Central issues an
 * ATT_READ_REQ or ATT_READ_BLOB_REQ targeting the corresponding
 * characteristic value attribute.
 *
 * Parameters:
 *   conn   — the connection the ATT request arrived on
 *   attr   — the attribute being read (carries permissions and UUID)
 *   buf    — output buffer; write the characteristic value here
 *   len    — capacity of buf in bytes (at most ATT_MTU - 1)
 *   offset — byte offset for ATT long reads (ATT_READ_BLOB_REQ); allows
 *             the Central to read values larger than a single MTU payload
 *
 * bt_gatt_attr_read() is the recommended helper.  It copies the correct
 * slice of value[offset .. offset+len] into buf and returns the byte count.
 * Always use it; handling offset manually is error-prone.
 *
 * Return value:
 *   >= 0  — bytes written into buf; the stack sends ATT_READ_RSP
 *   < 0   — negative errno; the stack sends ATT_ERROR_RSP to the Central
 * --------------------------------------------------------------------------
 */

static ssize_t read_fw_ver(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	/* Exclude the null terminator; ATT string values are not null-terminated
	 * on the wire — the Central uses the length from ATT_READ_RSP instead.
	 */
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 fw_ver_str, strlen(fw_ver_str));
}

static ssize_t read_led_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &led_ctrl_val, sizeof(led_ctrl_val));
}

static ssize_t read_counter(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    void *buf, uint16_t len, uint16_t offset)
{
	/* counter_val is stored as a native little-endian uint16_t.
	 * Cortex-M is little-endian, which matches the BLE on-wire byte order,
	 * so no endian conversion is needed.
	 */
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &counter_val, sizeof(counter_val));
}

/* --------------------------------------------------------------------------
 * ATT Write callback — LED Control
 * --------------------------------------------------------------------------
 * The GATT server calls this whenever a Central issues ATT_WRITE_REQ to the
 * LED Control characteristic value attribute.
 *
 * Parameters:
 *   conn   — the connection that sent the write
 *   attr   — the attribute being written
 *   buf    — the raw bytes the Central sent (the new value)
 *   len    — number of bytes in buf
 *   offset — byte offset for ATT long writes (ATT_PREPARE_WRITE_REQ);
 *             always 0 for ordinary ATT_WRITE_REQ
 *   flags  — BT_GATT_WRITE_FLAG_PREPARE (long-write prepare phase) or 0
 *
 * Return value:
 *   >= 0  — number of bytes accepted; usually == len
 *   < 0   — negative errno; the stack sends ATT_ERROR_RSP
 *
 * BT_GATT_ERR(code) constructs the negative error value expected by the
 * stack from a BT_ATT_ERR_* constant.  The stack translates it back to an
 * ATT error code in the ATT_ERROR_RSP PDU.
 * --------------------------------------------------------------------------
 */
static ssize_t write_led_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len, uint16_t offset,
			      uint8_t flags)
{
	const uint8_t *data = buf;

	/* Reject long-write prepare phases; this characteristic is too small
	 * to need ATT long-write reassembly.
	 */
	if (offset != 0 || len != sizeof(led_ctrl_val)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	/* Only 0x00 (OFF) and 0x01 (ON) are valid values; reject anything else
	 * with ATT error 0x13 "Value Not Allowed".
	 */
	if (data[0] > 1U) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	led_ctrl_val = data[0];

	LOG_INF("LED control written: %s (0x%02x)",
		led_ctrl_val ? "ON" : "OFF", led_ctrl_val);

	return (ssize_t)len;
}

/* --------------------------------------------------------------------------
 * CCC Changed callback — Counter characteristic
 * --------------------------------------------------------------------------
 * The GATT server calls this whenever a Central writes to the CCC descriptor
 * (UUID 0x2902) of the Counter characteristic.
 *
 * The CCC descriptor is per-connection and per-characteristic.  After a
 * disconnect the Central must re-subscribe; the Zephyr stack resets the CCC
 * value automatically on disconnect.
 *
 * value:
 *   BT_GATT_CCC_NOTIFY (0x0001) — Central subscribed to notifications
 *   0x0000                      — Central unsubscribed
 * --------------------------------------------------------------------------
 */
static void counter_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	atomic_set(&notify_enabled, enabled ? 1 : 0);

	LOG_INF("counter notifications %s", enabled ? "enabled" : "disabled");
}

/* --------------------------------------------------------------------------
 * GATT Service definition
 * --------------------------------------------------------------------------
 * BT_GATT_SERVICE_DEFINE() compiles the attribute table into a dedicated
 * linker section (iterable section "bt_gatt_service_static").  The GATT
 * server iterates over all registered services at boot and assigns handles
 * sequentially, starting from 1.
 *
 * No runtime registration call is needed; the service is live as soon as
 * Bluetooth is enabled with bt_enable().
 *
 * Attribute table indices (0-based, fixed at compile time):
 *   [0] Primary Service declaration        (UUID 0x2800)
 *   [1] FW Version  char declaration       (UUID 0x2803)
 *   [2] FW Version  char value             (UUID gatt_demo_fw_ver_uuid)
 *   [3] LED Control char declaration       (UUID 0x2803)
 *   [4] LED Control char value             (UUID gatt_demo_led_ctrl_uuid)
 *   [5] Counter     char declaration       (UUID 0x2803)
 *   [6] Counter     char value             (UUID gatt_demo_counter_uuid)
 *   [7] Counter     CCC descriptor         (UUID 0x2902)
 *
 * The Counter value is at index 6; this index is passed to bt_gatt_notify()
 * so the stack knows which ATT handle to use in ATT_HANDLE_VALUE_NTF.
 * --------------------------------------------------------------------------
 */

/* Attribute table index of the Counter characteristic value. */
#define GATT_DEMO_COUNTER_ATTR_IDX  6

BT_GATT_SERVICE_DEFINE(gatt_demo_svc,
	/* --- Primary Service declaration -----------------------------------
	 * Marks the start of the service in the attribute table.
	 * The service UUID is stored as the attribute value so the Central can
	 * retrieve it during GATT service discovery (ATT_READ_BY_GROUP_TYPE).
	 */
	BT_GATT_PRIMARY_SERVICE(&gatt_demo_svc_uuid.uuid),

	/* --- Characteristic 1: Firmware Version (Read-only) ----------------
	 *
	 * Properties: BT_GATT_CHRC_READ
	 *   The Central may issue ATT_READ_REQ; ATT_WRITE_REQ will be
	 *   rejected by the stack with ATT_ERROR_RSP(Write Not Permitted).
	 *
	 * Permissions: BT_GATT_PERM_READ
	 *   The stack checks permissions before invoking the read callback.
	 *   No encryption or authentication required in this demo.
	 */
	BT_GATT_CHARACTERISTIC(&gatt_demo_fw_ver_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_fw_ver, NULL, NULL),

	/* --- Characteristic 2: LED Control (Read / Write) ------------------
	 *
	 * Properties: BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE
	 *   Both ATT_READ_REQ and ATT_WRITE_REQ are accepted.
	 *   (BT_GATT_CHRC_WRITE requires ATT_WRITE_RSP acknowledgement;
	 *    use BT_GATT_CHRC_WRITE_WITHOUT_RESP for fire-and-forget writes.)
	 *
	 * Permissions: BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
	 *   Write allowed without link-layer encryption.  Production firmware
	 *   should use BT_GATT_PERM_WRITE_ENCRYPT to require an encrypted link.
	 */
	BT_GATT_CHARACTERISTIC(&gatt_demo_led_ctrl_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_led_ctrl, write_led_ctrl, NULL),

	/* --- Characteristic 3: Counter (Read / Notify) ---------------------
	 *
	 * Properties: BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY
	 *   The Central can read the current value at any time.
	 *   The Server pushes ATT_HANDLE_VALUE_NTF when the counter increments,
	 *   but only after the Central has written 0x0001 to the CCC descriptor.
	 *
	 * The BT_GATT_CCC macro that follows this characteristic adds the CCC
	 * descriptor automatically.  It must appear immediately after the
	 * characteristic value attribute.
	 */
	BT_GATT_CHARACTERISTIC(&gatt_demo_counter_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ,
		read_counter, NULL, NULL),

	/* --- CCC descriptor for Counter ------------------------------------
	 * counter_ccc_changed() is invoked each time the Central writes a new
	 * subscription state.
	 *
	 * BT_GATT_PERM_READ | BT_GATT_PERM_WRITE: the Central must be able to
	 * both read the current CCC state and write a new subscription value.
	 */
	BT_GATT_CCC(counter_ccc_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* --------------------------------------------------------------------------
 * Notification thread
 * --------------------------------------------------------------------------
 * Increments counter_val every NOTIFY_INTERVAL_MS milliseconds and pushes an
 * ATT_HANDLE_VALUE_NTF to the connected Central (if it has subscribed via the
 * CCC descriptor).
 *
 * bt_gatt_notify() arguments:
 *   conn  — target connection, or NULL to notify all connected clients
 *   attr  — pointer to the characteristic value attribute in the GATT table
 *   data  — pointer to the new value bytes
 *   len   — number of bytes to send
 *
 * Common return values:
 *   0          — notification queued successfully
 *   -ENOTCONN  — no active connection (normal when device is advertising)
 *   -EPERM     — the Central has not enabled notifications (CCC = 0x0000)
 * --------------------------------------------------------------------------
 */
static void gatt_demo_notify_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msleep(CONFIG_PACKAGE_GATT_SERVER_DEMO_NOTIFY_INTERVAL_MS);

		counter_val++;

		/* Guard: skip the BT stack call when no client has subscribed.
		 * This avoids unnecessary overhead and suppresses -EPERM noise.
		 */
		if (atomic_get(&notify_enabled) == 0) {
			continue;
		}

		int err = bt_gatt_notify(NULL,
					 &gatt_demo_svc.attrs[GATT_DEMO_COUNTER_ATTR_IDX],
					 &counter_val,
					 sizeof(counter_val));
		if (err) {
			/* -ENOTCONN is normal if the connection dropped between
			 * the notify_enabled check and the notify call.
			 */
			LOG_DBG("counter notify failed (%d)", err);
		} else {
			LOG_DBG("counter notify sent: %u", counter_val);
		}
	}
}

K_THREAD_DEFINE(gatt_demo_notify_tid,
		CONFIG_PACKAGE_GATT_SERVER_DEMO_STACK_SIZE,
		gatt_demo_notify_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_GATT_SERVER_DEMO_THREAD_PRIORITY,
		0, 0);

/* --------------------------------------------------------------------------
 * Advertising data
 * --------------------------------------------------------------------------
 * Including the 128-bit service UUID in the advertising payload lets scanner
 * apps (e.g. nRF Connect) filter for this specific GATT service without
 * connecting first.
 *
 * AD budget: 31 bytes maximum
 *   Flags record:       3 bytes  (type + length + value)
 *   UUID128_ALL record: 18 bytes (2 overhead + 16 bytes UUID)
 *   Total:              21 bytes — within budget.
 *
 * The device name is placed in the Scan Response instead of the primary AD
 * to stay within the 31-byte advertising packet limit.
 * --------------------------------------------------------------------------
 */

/* Raw little-endian byte sequence of the service UUID for the AD record. */
static const uint8_t gatt_demo_svc_uuid_ad[] = {GATT_DEMO_SVC_UUID_VAL};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	/* Advertise the full list of 128-bit service UUIDs so that
	 * GATT-aware scanners can identify the demo service at discovery time.
	 */
	BT_DATA(BT_DATA_UUID128_ALL,
		gatt_demo_svc_uuid_ad,
		sizeof(gatt_demo_svc_uuid_ad)),
};

static const struct bt_data sd[] = {
	/* Complete device name in the Scan Response (returned after an active
	 * Scan Request from the Central).
	 */
	BT_DATA(BT_DATA_NAME_COMPLETE,
		CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Forward declaration. */
static void start_advertising(void);

static void start_advertising(void)
{
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
			      ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("advertising start failed (%d)", err);
		return;
	}

	LOG_INF("advertising started — GATT server ready, waiting for connection");
}

/* --------------------------------------------------------------------------
 * Connection lifecycle callbacks
 * --------------------------------------------------------------------------
 */
static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_ERR("connection to [%s] failed (err 0x%02x)", addr, conn_err);
		return;
	}

	/* Retain a reference so we can call bt_conn_disconnect() if needed. */
	current_conn = bt_conn_ref(conn);

	LOG_INF("connected to [%s]", addr);
	LOG_INF("GATT server active — use nRF Connect to explore the service:");
	LOG_INF("  Read  Firmware Version  -> \"1.0.0-demo\"");
	LOG_INF("  Write LED Control       -> 0x00 (OFF) or 0x01 (ON)");
	LOG_INF("  Subscribe Counter       -> increments every %d ms",
		CONFIG_PACKAGE_GATT_SERVER_DEMO_NOTIFY_INTERVAL_MS);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("disconnected from [%s] (reason 0x%02x)", addr, reason);

	/* Reset notification subscription state on disconnect.  The CCC value
	 * is per-connection; the next client must re-subscribe after connecting.
	 * The Zephyr stack also resets its internal CCC copy automatically.
	 */
	atomic_set(&notify_enabled, 0);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	start_advertising();
}

BT_CONN_CB_DEFINE(gatt_demo_conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
};

/* --------------------------------------------------------------------------
 * Module initialisation
 * --------------------------------------------------------------------------
 */
static int gatt_server_demo_init(void)
{
	int err;

	/* bt_enable() initialises the BLE Host + Controller stack.
	 * Passing NULL selects synchronous init: the function blocks until the
	 * stack is fully ready before returning.
	 *
	 * Important: bt_enable() must be called exactly once in the firmware
	 * image. If another BLE demo is also enabled it will return an error
	 * here — enable only one BLE demo module at a time in prj.conf.
	 */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialised — GATT server demo starting");

	start_advertising();

	return 0;
}

SYS_INIT(gatt_server_demo_init, APPLICATION,
	 CONFIG_PACKAGE_GATT_SERVER_DEMO_INIT_PRIORITY);
