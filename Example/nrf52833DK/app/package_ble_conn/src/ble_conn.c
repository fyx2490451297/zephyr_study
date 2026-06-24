#include "ble_conn.h"
#include "ble_adv.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_conn, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE Connection Manager
 * --------------------------------------------------------------------------
 * This module owns the BT_CONN_CB_DEFINE registration — only one module
 * should own this to avoid duplicated callbacks.  Other modules (gatt
 * services, OTA service) that need connection events register through
 * ble_conn_register_event_cb() and receive a dispatched notification.
 *
 * Connection lifecycle:
 *
 *   1. Device advertises (started by ble_adv at priority 91).
 *   2. Central connects → connected() fires:
 *        - refs the conn handle for external use
 *        - dispatches BLE_CONN_EVT_CONNECTED to all registered callbacks
 *   3. Central disconnects → disconnected() fires:
 *        - unrefs the conn handle
 *        - dispatches BLE_CONN_EVT_DISCONNECTED
 *        - calls ble_adv_start() to re-enter advertising state
 *
 * Thread safety:
 *   All BT callbacks run on the BT RX work queue, not an ISR.  The
 *   cb_mutex protects the callback table from races with registration calls
 *   that could come from application threads.
 * --------------------------------------------------------------------------
 */

/* --------------------------------------------------------------------------
 * Callback table
 * --------------------------------------------------------------------------
 */
static struct {
	ble_conn_event_cb_t cb;
	void               *user_data;
} event_cbs[CONFIG_PACKAGE_BLE_CONN_MAX_EVENT_CBS];

static uint8_t      event_cb_count;
static struct k_mutex cb_mutex;

int ble_conn_register_event_cb(ble_conn_event_cb_t cb, void *user_data)
{
	if (cb == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&cb_mutex, K_FOREVER);

	if (event_cb_count >= CONFIG_PACKAGE_BLE_CONN_MAX_EVENT_CBS) {
		k_mutex_unlock(&cb_mutex);
		LOG_ERR("event callback table full (%d slots)",
			CONFIG_PACKAGE_BLE_CONN_MAX_EVENT_CBS);
		return -ENOMEM;
	}

	event_cbs[event_cb_count].cb        = cb;
	event_cbs[event_cb_count].user_data = user_data;
	event_cb_count++;

	k_mutex_unlock(&cb_mutex);

	return 0;
}

/* --------------------------------------------------------------------------
 * Active connection state
 * --------------------------------------------------------------------------
 */
static struct bt_conn *active_conn;

struct bt_conn *ble_conn_get_active(void)
{
	return active_conn;
}

int ble_conn_disconnect(void)
{
	if (active_conn == NULL) {
		return -ENOTCONN;
	}

	return bt_conn_disconnect(active_conn,
				  BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

/* --------------------------------------------------------------------------
 * Internal dispatch helper
 * --------------------------------------------------------------------------
 */
static void dispatch_event(ble_conn_evt_t evt, struct bt_conn *conn)
{
	k_mutex_lock(&cb_mutex, K_FOREVER);

	for (uint8_t i = 0; i < event_cb_count; i++) {
		if (event_cbs[i].cb != NULL) {
			event_cbs[i].cb(evt, conn, event_cbs[i].user_data);
		}
	}

	k_mutex_unlock(&cb_mutex);
}

/* --------------------------------------------------------------------------
 * BT connection callbacks
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

	/* Retain a reference so callers of ble_conn_get_active() receive a
	 * valid pointer.  Released in disconnected() below.
	 */
	active_conn = bt_conn_ref(conn);

	LOG_INF("connected to [%s]", addr);

	dispatch_event(BLE_CONN_EVT_CONNECTED, conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	/* Common disconnect reason codes (hci_err.h):
	 *   0x08  Connection Timeout
	 *   0x13  Remote User Terminated Connection
	 *   0x16  Local Host Terminated Connection
	 */
	LOG_INF("disconnected from [%s] (reason 0x%02x)", addr, reason);

	dispatch_event(BLE_CONN_EVT_DISCONNECTED, conn);

	if (active_conn) {
		bt_conn_unref(active_conn);
		active_conn = NULL;
	}

	/* Re-enter advertising state so the next Central can connect. */
	ble_adv_start();
}

BT_CONN_CB_DEFINE(ble_conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
};

/* --------------------------------------------------------------------------
 * Module initialisation
 * --------------------------------------------------------------------------
 */
static int ble_conn_init(void)
{
	k_mutex_init(&cb_mutex);

	/* BT_CONN_CB_DEFINE registers the callbacks statically at link time;
	 * no runtime registration call is needed here.
	 * Advertising was already started by ble_adv (priority 91).
	 */
	LOG_INF("connection manager initialised (max %d event callbacks)",
		CONFIG_PACKAGE_BLE_CONN_MAX_EVENT_CBS);

	return 0;
}

SYS_INIT(ble_conn_init, APPLICATION, CONFIG_PACKAGE_BLE_CONN_INIT_PRIORITY);
