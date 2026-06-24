#ifndef BLE_CONN_H_
#define BLE_CONN_H_

#include <stdint.h>
#include <zephyr/bluetooth/conn.h>

/* --------------------------------------------------------------------------
 * BLE Connection event types
 * --------------------------------------------------------------------------
 */
typedef enum {
	BLE_CONN_EVT_CONNECTED    = 0, /* A Central has connected.           */
	BLE_CONN_EVT_DISCONNECTED = 1, /* The active connection was lost.    */
} ble_conn_evt_t;

/**
 * @brief Connection event callback type.
 *
 * Invoked from the BT RX work queue (not ISR context) on each connection
 * state change.  Kernel APIs (logging, semaphores, etc.) are safe to call.
 *
 * @param evt       Type of event (connected or disconnected).
 * @param conn      Opaque connection handle; do NOT call bt_conn_ref() or
 *                  bt_conn_unref() on this pointer inside the callback.
 *                  The reference is managed by ble_conn internally.
 * @param user_data Opaque pointer supplied at registration time.
 */
typedef void (*ble_conn_event_cb_t)(ble_conn_evt_t evt,
				    struct bt_conn *conn,
				    void *user_data);

/**
 * @brief Register a connection event callback.
 *
 * Up to CONFIG_PACKAGE_BLE_CONN_MAX_EVENT_CBS callbacks may be registered.
 * Callbacks are dispatched in registration order.  May be called before or
 * after ble_conn_init() — registrations before the first connection event
 * are always honoured.
 *
 * @param cb        Callback function; must not be NULL.
 * @param user_data Opaque pointer passed back in every callback invocation.
 *
 * @return 0 on success, -ENOMEM if the callback table is full.
 */
int ble_conn_register_event_cb(ble_conn_event_cb_t cb, void *user_data);

/**
 * @brief Get the currently active BLE connection handle.
 *
 * @return Pointer to the active bt_conn, or NULL if not connected.
 *         The returned pointer is valid only until a disconnection event.
 *         Do NOT call bt_conn_unref() on the returned pointer.
 */
struct bt_conn *ble_conn_get_active(void);

/**
 * @brief Disconnect the active BLE connection.
 *
 * Issues a disconnect with reason BT_HCI_ERR_REMOTE_USER_TERM_CONN.
 * Has no effect if not connected.
 *
 * @return 0 on success, -ENOTCONN if no active connection, or a negative
 *         errno on failure.
 */
int ble_conn_disconnect(void);

#endif /* BLE_CONN_H_ */
