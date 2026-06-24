/* No public API for this demo module. */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_adv_conn_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE Advertising + Connection Demo — Peripheral Role
 * --------------------------------------------------------------------------
 *
 * BLE devices have two fundamental GAP roles:
 *
 *   Peripheral:
 *     Continuously broadcasts Advertising packets and waits for a Central
 *     to initiate a connection. Operates as a Slave after connection and
 *     typically hosts a GATT Server.
 *     -> This module demonstrates the full lifecycle of this role.
 *
 *   Central:
 *     Scans for Advertising packets and actively initiates connections.
 *     -> See package_ble_scan_conn_demo.
 *
 * Full flow demonstrated by this module
 * ──────────────────────────────────────
 *
 *  ┌──────────────────────────────────────────────────────────────────┐
 *  │  1. bt_enable()           Initialise BLE Host + Controller stack │
 *  │  2. bt_le_adv_start()     Start advertising, announce presence   │
 *  │  3. <- Central scans and sends a connection request              │
 *  │  4. connected()           Callback: connection established       │
 *  │     └─ bt_conn_get_info() Read and log connection parameters     │
 *  │  5. le_param_req()        Callback: Central requests param update│
 *  │  6. le_param_updated()    Callback: parameter negotiation done   │
 *  │  7. disconnected()        Callback: link dropped, restart adv    │
 *  │  8. -> Back to step 2, loop waiting for the next connection      │
 *  └──────────────────────────────────────────────────────────────────┘
 *
 * Key data structures
 * ───────────────────
 *   struct bt_data  — One AD (Advertising Data) record:
 *                     [1 byte Length][1 byte Type][N bytes Payload]
 *                     An advertising packet is composed of several bt_data
 *                     records.
 *
 *   struct bt_conn  — Opaque handle representing a BLE connection, managed
 *                     by reference counting. The conn parameter passed to
 *                     connected() already holds one reference which is
 *                     released automatically after disconnected() returns.
 *                     Any extra bt_conn_ref() call must be paired with a
 *                     bt_conn_unref() call to avoid a connection-slot leak.
 *
 * Advertising Data vs. Scan Response Data
 * ─────────────────────────────────────────
 *   Advertising Data (ad[])  : Sent in every advertising event, visible to
 *                              all scanners.
 *   Scan Response Data (sd[]): Returned only after an active Scan Request
 *                              from the Central; used for extra payload that
 *                              does not fit in the advertising packet.
 * --------------------------------------------------------------------------
 */

/* --------------------------------------------------------------------------
 * Advertising Data
 * --------------------------------------------------------------------------
 * BT_DATA_BYTES / BT_DATA macros construct TLV (Type-Length-Value) AD
 * records that are packed into the 31-byte advertising payload.
 * --------------------------------------------------------------------------
 */
static const struct bt_data ad[] = {
	/* Flags: declare the discoverable mode and transport support.
	 *
	 * BT_LE_AD_GENERAL  — General Discoverable Mode: the device can be
	 *                     discovered by any Central with no time limit.
	 *                     (BT_LE_AD_LIMITED caps discovery at 30.72 s.)
	 *
	 * BT_LE_AD_NO_BREDR — Declares BLE-only; no Classic Bluetooth (BR/EDR).
	 *                     The nRF52833 is BLE-only, so this flag is mandatory.
	 */
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),

	/* Complete Local Name: device name visible directly to scanner apps.
	 *
	 * Note: Legacy Advertising payload is limited to 31 bytes total.
	 * If the name is too long, use BT_DATA_NAME_SHORTENED with truncation,
	 * or move the full name into the Scan Response packet.
	 */
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* --------------------------------------------------------------------------
 * Scan Response Data
 * --------------------------------------------------------------------------
 * The Scan Response packet has the same format as the advertising packet
 * but is only returned after an active Scan Request from the Central.
 * Manufacturer Specific Data is placed here to demonstrate Scan Response
 * usage.
 *
 * Manufacturer Specific Data format (BT_DATA_MANUFACTURER_DATA):
 *   [2 bytes Company ID (little-endian)] [arbitrary payload]
 *   0xFF, 0xFF = reserved ID for testing/study purposes.
 *   (Production devices must obtain an official Company ID from the
 *   Bluetooth SIG.)
 * --------------------------------------------------------------------------
 */
static const uint8_t manuf_data[] = {
	0xFF, 0xFF,           /* Company ID: 0xFFFF (test placeholder) */
	'Z', 'E', 'P', 'H',   /* Custom payload: application identifier  */
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, manuf_data, sizeof(manuf_data)),
};

/* --------------------------------------------------------------------------
 * Advertising parameters
 * --------------------------------------------------------------------------
 * BT_LE_ADV_CONN_FAST_1 is a Zephyr predefined "fast connectable" parameter
 * set:
 *   - Advertising Interval: 30 ~ 60 ms
 *     Shorter interval means faster discovery but higher power consumption.
 *     Longer interval reduces power but slows discovery.
 *   - Advertising type: ADV_IND (undirected connectable advertising)
 *     Any Central may initiate a connection to this device.
 *
 * Other commonly used predefined parameter sets:
 *   BT_LE_ADV_CONN_SLOW      — Slow advertising (1 ~ 1.2 s), low-power use.
 *   BT_LE_ADV_NCONN_IDENTITY — Non-connectable advertising for beacons.
 *
 * For full custom parameters, fill in a struct bt_le_adv_param manually.
 * --------------------------------------------------------------------------
 */

/* Active connection handle; NULL when not connected.
 * Retained so the connection can be terminated programmatically if needed
 * (this demo only demonstrates the passive disconnect scenario).
 */
static struct bt_conn *current_conn;

/* Forward declaration: disconnected() callback needs to call this. */
static void start_advertising(void);

/* --------------------------------------------------------------------------
 * Advertising helper
 * --------------------------------------------------------------------------
 */
static void start_advertising(void)
{
	int err;

	/* bt_le_adv_start() arguments:
	 *   param   — advertising parameters (interval, type, etc.)
	 *   ad      — advertising data array
	 *   ad_len  — number of advertising data records
	 *   sd      — scan response data array (NULL to omit)
	 *   sd_len  — number of scan response data records
	 *
	 * The BLE stack stops advertising automatically once a connection is
	 * established; this function must be called again after disconnection.
	 */
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
			      ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("advertising start failed (%d)", err);
		return;
	}

	LOG_INF("advertising started — waiting for connection...");
}

/* --------------------------------------------------------------------------
 * Connection lifecycle callbacks
 *
 * BT_CONN_CB_DEFINE registers these functions with the BLE Host connection
 * management subsystem. Callbacks run on the BT RX work queue (not an ISR
 * context), so kernel APIs may be called safely.
 * --------------------------------------------------------------------------
 */

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn_info info;
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		/* Connection establishment failed. This is uncommon on the
		 * Peripheral side but can occur on directed advertising timeout.
		 */
		LOG_ERR("connection to [%s] failed (err 0x%02x)", addr, conn_err);
		return;
	}

	/* Retain the connection handle for use outside this callback.
	 *
	 * The conn pointer passed to connected() is owned by the stack for the
	 * duration of the callback. To continue using it afterwards, call
	 * bt_conn_ref() to increment the reference count, then bt_conn_unref()
	 * when done.
	 */
	current_conn = bt_conn_ref(conn);

	LOG_INF("connected to [%s]", addr);

	/* Read and log the negotiated connection parameters.
	 *
	 * Connection Parameters are proposed by the Central; the Peripheral
	 * can accept or reject them via the le_param_req() callback:
	 *
	 *   interval — Connection Interval, unit 1.25 ms.
	 *              Range: 6 (7.5 ms) to 3200 (4 s).
	 *              How often Central and Peripheral communicate.
	 *              Shorter interval = lower latency, higher power.
	 *
	 *   latency  — Slave Latency: maximum number of consecutive connection
	 *              events the Peripheral may skip.
	 *              0 = must respond every event (lowest latency).
	 *              >0 = Peripheral may idle to save power.
	 *
	 *   timeout  — Supervision Timeout, unit 10 ms.
	 *              If no packet is received within this window, the link is
	 *              declared lost and disconnected() is triggered.
	 */
	err = bt_conn_get_info(conn, &info);
	if (err == 0 && info.type == BT_CONN_TYPE_LE) {
		LOG_INF("conn params: interval %u*1.25ms  latency %u  timeout %u*10ms",
			info.le.interval,
			info.le.latency,
			info.le.timeout);
	}

	/* GATT service discovery and notification subscriptions would be
	 * initiated here. This demo only covers the connection setup flow
	 * and does not perform any GATT interaction.
	 */
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	/* Common disconnection reason codes (defined in hci_err.h):
	 *   0x08 — Connection Timeout (Supervision Timeout expired)
	 *   0x13 — Remote User Terminated Connection
	 *   0x16 — Local Host Terminated Connection
	 *   0x22 — LMP / LL Response Timeout
	 *   0x3B — Connection Failed to be Established
	 */
	LOG_INF("disconnected from [%s] (reason 0x%02x)", addr, reason);

	/* Release the reference acquired by bt_conn_ref() in connected().
	 *
	 * Reference counting rules:
	 *   bt_conn_ref()   increments refcount
	 *   bt_conn_unref() decrements refcount; the internal slot is freed
	 *                   when it reaches zero.
	 * Mismatched calls cause a connection-slot leak, eventually causing
	 * new connection requests to be rejected.
	 */
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	/* Restart advertising after disconnection to wait for the next
	 * connection — the standard behaviour pattern for a Peripheral.
	 */
	start_advertising();
}

/* --------------------------------------------------------------------------
 * Connection parameter update callbacks
 *
 * le_param_req() — Triggered when the Central requests a parameter update.
 *   Return true  -> Peripheral accepts the proposed parameters.
 *   Return false -> Peripheral rejects; Central keeps the current parameters.
 *
 * In production firmware, validate here whether the proposed parameters
 * satisfy the application's latency and power budget. This demo accepts
 * all valid requests unconditionally.
 * --------------------------------------------------------------------------
 */
static bool le_param_req(struct bt_conn *conn,
			 struct bt_le_conn_param *param)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("conn param req from [%s]: interval %u~%u  latency %u  timeout %u",
		addr,
		param->interval_min,
		param->interval_max,
		param->latency,
		param->timeout);

	/* Accept the parameters proposed by the Central. */
	return true;
}

/* le_param_updated() — Triggered after negotiation completes; logs the
 * parameters that are now in effect.
 */
static void le_param_updated(struct bt_conn *conn,
			     uint16_t interval,
			     uint16_t latency,
			     uint16_t timeout)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("conn params updated [%s]: interval %u*1.25ms  latency %u  timeout %u*10ms",
		addr, interval, latency, timeout);
}

/* --------------------------------------------------------------------------
 * Register the connection callback set
 *
 * BT_CONN_CB_DEFINE() is the modern Zephyr registration mechanism
 * (supersedes the legacy bt_conn_cb_register() function call). The macro
 * places the struct in a dedicated linker section; the stack iterates over
 * all registered callback sets automatically during initialisation.
 * --------------------------------------------------------------------------
 */
BT_CONN_CB_DEFINE(adv_conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
	.le_param_req     = le_param_req,
	.le_param_updated = le_param_updated,
};

/* --------------------------------------------------------------------------
 * Module initialisation entry point
 * --------------------------------------------------------------------------
 */
static int ble_adv_conn_demo_init(void)
{
	int err;

	/* Step 1: Initialise the BLE stack (Host + Controller).
	 *
	 * Passing NULL means synchronous init: bt_enable() blocks until the
	 * stack is ready. Passing a callback pointer enables asynchronous init
	 * and invokes the callback once the stack is ready.
	 *
	 * Note: bt_enable() must only be called once per firmware image. If
	 * another BLE demo module (e.g. package_ble_adv_demo) is enabled at the
	 * same time, the second call will return -EALREADY and be silently
	 * ignored. Enable only one BLE demo module at a time via prj.conf.
	 */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialised");

	/* Step 2: Start advertising and enter the connection-wait state. */
	start_advertising();

	return 0;
}

/* SYS_INIT invokes ble_adv_conn_demo_init() automatically during the
 * APPLICATION init phase — no manual call from main() is needed.
 *
 * Priority 90 is the recommended value for BLE modules, ensuring the stack
 * starts after driver-level (40) and kernel-object (50) initialisation.
 */
SYS_INIT(ble_adv_conn_demo_init, APPLICATION,
	 CONFIG_PACKAGE_ADV_CONN_DEMO_INIT_PRIORITY);
