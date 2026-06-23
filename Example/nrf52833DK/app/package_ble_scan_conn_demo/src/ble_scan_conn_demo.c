#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_scan_conn_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE Scan & Connection learning notes
 * --------------------------------------------------------------------------
 * BLE devices operate in one of two roles:
 *
 *   Peripheral (Advertiser):
 *     Broadcasts advertising packets so others can find it.
 *     Waits for a Central to initiate a connection.
 *     → See package_ble_adv_demo for this side.
 *
 *   Central (Scanner / Initiator):
 *     Actively scans the air for advertising packets.
 *     Chooses a device and initiates a connection.
 *     → THIS demo implements the Central role.
 *
 * The full flow this demo exercises:
 *
 *   1. bt_enable()          — initialise the BLE host + controller stack
 *   2. bt_le_scan_start()   — turn on the radio in scanner mode
 *   3. scan_cb()            — called for every advertising report received
 *   4. bt_data_parse()      — decode the AD structures inside the payload
 *   5. bt_le_scan_stop()    — stop scanning before connecting (required)
 *   6. bt_conn_le_create()  — send a connection request to the target
 *   7. connected()          — callback: connection succeeded or failed
 *   8. disconnected()       — callback: link was lost; restart from step 2
 *
 * Key API objects:
 *   struct bt_conn   — opaque handle that represents one BLE connection.
 *                      It is reference-counted: bt_conn_le_create() gives
 *                      you a reference (+1); call bt_conn_unref() when you
 *                      no longer need it to avoid a memory leak.
 *
 *   struct bt_data   — one AD (Advertising Data) structure:
 *                      [1-byte length][1-byte type][N-byte payload]
 *
 * Active vs Passive scan:
 *   BT_LE_SCAN_ACTIVE  — the scanner also sends Scan Request packets.
 *                        The advertiser may reply with a Scan Response,
 *                        which can carry extra data (e.g. a longer name).
 *   BT_LE_SCAN_PASSIVE — listen only, no Scan Requests sent.
 *                        Lower power but you only see the primary AD payload.
 * --------------------------------------------------------------------------
 */

/* Name of the peripheral we want to connect to.
 * This matches the name broadcast by package_ble_adv_demo.
 */
#define TARGET_NAME     "This Device"
#define TARGET_NAME_LEN (sizeof(TARGET_NAME) - 1)

/* Active connection handle.
 * NULL  → not connected.
 * !NULL → holds a reference obtained from bt_conn_le_create().
 */
static struct bt_conn *active_conn;

/* Forward declaration so scan_cb() can call start_scan() on error. */
static void start_scan(void);

/* --------------------------------------------------------------------------
 * AD payload parser callback
 *
 * bt_data_parse() walks through every AD structure in the raw advertising
 * payload and calls this function once per structure.
 *
 * Return value:
 *   true  → continue iterating to the next AD structure
 *   false → stop iteration early (we already found what we need)
 * --------------------------------------------------------------------------
 */
static bool ad_name_match_cb(struct bt_data *data, void *user_data)
{
	bool *match = user_data;

	/* We only care about the device name AD types.
	 * Skip anything else (flags, UUIDs, TX power, …).
	 */
	if (data->type != BT_DATA_NAME_COMPLETE &&
	    data->type != BT_DATA_NAME_SHORTENED) {
		return true; /* not a name field — keep iterating */
	}

	/* Compare the name payload to our target string. */
	if (data->data_len == TARGET_NAME_LEN &&
	    memcmp(data->data, TARGET_NAME, TARGET_NAME_LEN) == 0) {
		*match = true;
		return false; /* match found — stop iterating */
	}

	return true; /* name didn't match — keep iterating */
}

/* --------------------------------------------------------------------------
 * Scan callback
 *
 * The BLE controller calls this function for every advertising report it
 * receives while scanning.  We inspect each report to decide whether it
 * belongs to our target device.
 *
 * Parameters:
 *   addr  — BLE address of the advertiser (public or random)
 *   rssi  — received signal strength in dBm (negative; closer → higher)
 *   type  — advertising event type (connectable, non-connectable, etc.)
 *   ad    — raw bytes of the advertising payload
 * --------------------------------------------------------------------------
 */
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
		    struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	/* Only process connectable advertising events.
	 *
	 * BT_GAP_ADV_TYPE_ADV_IND        : undirected connectable (most common)
	 * BT_GAP_ADV_TYPE_ADV_DIRECT_IND : directed connectable (to a specific
	 *                                   peer address)
	 *
	 * Non-connectable or scannable-only beacons cannot accept a connection
	 * request, so there is no point in trying to connect to them.
	 */
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}

	/* Parse the advertising payload and check for our target name.
	 *
	 * bt_data_parse() decodes the raw TLV (type-length-value) structures
	 * inside 'ad' and calls ad_name_match_cb() for each one.
	 */
	bool name_match = false;
	bt_data_parse(ad, ad_name_match_cb, &name_match);

	if (!name_match) {
		return; /* not our target device */
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	LOG_INF("scan: found \"%s\" at [%s] RSSI %d dBm",
		TARGET_NAME, addr_str, rssi);

	/* Stop scanning before initiating a connection.
	 *
	 * The BLE controller cannot scan and create a connection at the same
	 * time (in the classic advertising model).  We must stop the scanner
	 * first, then issue the connection request.
	 */
	err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("scan stop failed (%d)", err);
		return;
	}

	/* Send a connection request to the target device.
	 *
	 * bt_conn_le_create() is non-blocking: it queues the request in the
	 * controller and returns immediately.  The result is reported later
	 * through the connected() callback below.
	 *
	 * BT_CONN_LE_CREATE_CONN  : standard connection creation settings
	 *                           (scan interval/window used during initiation)
	 * BT_LE_CONN_PARAM_DEFAULT: default connection interval (7.5–30 ms),
	 *                           slave latency 0, supervision timeout 4 s.
	 *
	 * On success, active_conn receives a reference (+1 refcount).
	 * We must call bt_conn_unref() when the connection ends.
	 */
	err = bt_conn_le_create(addr,
				BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT,
				&active_conn);
	if (err) {
		LOG_ERR("connection create failed (%d), restarting scan", err);
		start_scan();
	}
}

/* --------------------------------------------------------------------------
 * Connection lifecycle callbacks
 *
 * BT_CONN_CB_DEFINE registers these callbacks with the Bluetooth stack.
 * They are invoked from the BT RX work queue, not from an ISR, so it is
 * safe to call logging and other kernel APIs here.
 * --------------------------------------------------------------------------
 */

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

	if (conn_err) {
		/* The connection attempt failed (e.g. the peer did not respond,
		 * or the supervision timeout expired during initiation).
		 *
		 * We must release the reference that bt_conn_le_create() gave us,
		 * then restart scanning to try again.
		 */
		LOG_ERR("connect to [%s] failed (err 0x%02x)", addr_str, conn_err);
		bt_conn_unref(active_conn);
		active_conn = NULL;
		start_scan();
		return;
	}

	/* Connection established successfully.
	 *
	 * active_conn already holds a valid reference from bt_conn_le_create().
	 * We keep it here so we could later call bt_conn_disconnect() to
	 * terminate the link intentionally (not needed in this minimal demo).
	 */
	LOG_INF("connected to [%s]", addr_str);

	/* At this point the link is up.  A real application would now:
	 *   - Initiate GATT service discovery  (bt_gatt_discover)
	 *   - Subscribe to notifications       (bt_gatt_subscribe)
	 *   - Read / write characteristics     (bt_gatt_read / bt_gatt_write)
	 *
	 * For this learning demo we simply log the event and wait.
	 */
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

	/* Common disconnect reason codes (defined in hci_err.h):
	 *   0x08  — connection timeout (supervision timeout expired)
	 *   0x13  — remote user terminated connection
	 *   0x16  — local host terminated connection
	 *   0x22  — LMP / LL response timeout
	 */
	LOG_INF("disconnected from [%s] (reason 0x%02x)", addr_str, reason);

	/* Release our reference to the connection object.
	 *
	 * Every bt_conn_le_create() or bt_conn_ref() call must be matched by
	 * exactly one bt_conn_unref().  Failing to do this leaks the internal
	 * bt_conn slot and eventually causes new connections to be rejected.
	 */
	if (active_conn) {
		bt_conn_unref(active_conn);
		active_conn = NULL;
	}

	/* Automatically restart scanning to reconnect. */
	start_scan();
}

/* Register the connection callbacks with the Bluetooth stack.
 *
 * BT_CONN_CB_DEFINE() is the modern macro-based registration approach.
 * It replaces the older bt_conn_cb_register() function call.
 */
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
};

/* --------------------------------------------------------------------------
 * Scan startup helper
 * --------------------------------------------------------------------------
 */
static void start_scan(void)
{
	int err;

	/* bt_le_scan_start() turns on the BLE controller's scanner.
	 *
	 * BT_LE_SCAN_ACTIVE: send Scan Request packets when an advertiser
	 * responds to them.  This retrieves the Scan Response payload, which
	 * may contain additional data that did not fit in the primary AD.
	 *
	 * The second argument is the per-report callback function.
	 */
	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, scan_cb);
	if (err) {
		LOG_ERR("scan start failed (%d)", err);
		return;
	}

	LOG_INF("scanning for \"%s\" ...", TARGET_NAME);
}

/* --------------------------------------------------------------------------
 * Module initialisation
 * --------------------------------------------------------------------------
 */
static int ble_scan_conn_demo_init(void)
{
	int err;

	/* Initialise the Bluetooth host and controller stack.
	 *
	 * Passing NULL means initialisation is synchronous: bt_enable() blocks
	 * until the stack is ready.  Passing a callback makes it asynchronous.
	 */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed (%d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialised — starting scan");

	start_scan();

	return 0;
}

SYS_INIT(ble_scan_conn_demo_init, APPLICATION,
	 CONFIG_PACKAGE_BLE_SCAN_CONN_DEMO_INIT_PRIORITY);
