#include "ble_gatt_client.h"
#include "ble_conn.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_gatt_client, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * BLE GATT Client implementation
 * --------------------------------------------------------------------------
 *
 * Zephyr GATT client API overview
 * ────────────────────────────────
 *
 *  bt_gatt_discover()   — async service / characteristic / descriptor scan.
 *                         Drives ATT_FIND_BY_TYPE_VALUE, ATT_READ_BY_TYPE,
 *                         or ATT_FIND_INFORMATION internally.
 *                         Calls the provided bt_gatt_discover_params.func
 *                         callback for each discovered attribute; callback
 *                         returns BT_GATT_ITER_CONTINUE or BT_GATT_ITER_STOP.
 *
 *  bt_gatt_read()       — async ATT_READ_REQ / ATT_READ_BLOB_REQ.
 *                         Calls bt_gatt_read_params.func on completion.
 *
 *  bt_gatt_write()      — async ATT_WRITE_REQ.
 *                         Calls bt_gatt_write_params.func on completion.
 *
 *  bt_gatt_write_without_response_cb() — ATT_WRITE_CMD (no response).
 *
 *  bt_gatt_subscribe()  — writes CCC to enable notifications and registers
 *                         a bt_gatt_subscribe_params.notify callback.
 *
 *  bt_gatt_unsubscribe() — writes CCC 0x0000 and removes the subscription.
 *
 * All _params structs MUST remain valid (not freed / overwritten) until the
 * associated callback fires.  This implementation uses static storage for
 * all params, serialises discovery/read/write, and pools subscriptions.
 *
 * Serialisation strategy:
 *   An atomic `op_busy` flag prevents two discovery or read/write operations
 *   from racing.  The flag is cleared in the callback before invoking the
 *   user callback so the user can immediately start a follow-up operation.
 * --------------------------------------------------------------------------
 */

/* --------------------------------------------------------------------------
 * Discovery state
 * --------------------------------------------------------------------------
 */
static struct bt_gatt_discover_params  disc_params;
static atomic_t                        disc_busy;

/* User callback and context stored so the internal Zephyr callback can
 * dispatch to the correct user-supplied function pointer.
 */
static struct {
	enum {
		DISC_TYPE_SERVICE,
		DISC_TYPE_CHAR,
		DISC_TYPE_DESC,
	} type;

	union {
		ble_gatt_client_service_found_cb_t service_cb;
		ble_gatt_client_char_found_cb_t    char_cb;
		ble_gatt_client_desc_found_cb_t    desc_cb;
	};

	void *user_data;
} disc_ctx;

static uint8_t on_discover(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr,
			   struct bt_gatt_discover_params *params)
{
	if (attr == NULL) {
		/* Discovery complete — no more attributes in range. */
		switch (disc_ctx.type) {
		case DISC_TYPE_SERVICE:
			/* Service not found or scan finished. */
			atomic_clear(&disc_busy);
			if (disc_ctx.service_cb) {
				disc_ctx.service_cb(conn, 0, 0, 0, disc_ctx.user_data);
			}
			break;
		case DISC_TYPE_CHAR:
			atomic_clear(&disc_busy);
			if (disc_ctx.char_cb) {
				disc_ctx.char_cb(conn, 0, 0, 0, disc_ctx.user_data);
			}
			break;
		case DISC_TYPE_DESC:
			/* Signal end-of-scan with handle = 0, uuid = NULL. */
			atomic_clear(&disc_busy);
			if (disc_ctx.desc_cb) {
				disc_ctx.desc_cb(conn, 0, NULL, 0, disc_ctx.user_data);
			}
			break;
		}
		return BT_GATT_ITER_STOP;
	}

	switch (disc_ctx.type) {
	case DISC_TYPE_SERVICE: {
		/* attr->user_data points to struct bt_gatt_service_val which
		 * contains the service UUID and its end_handle.
		 */
		const struct bt_gatt_service_val *svc =
			(const struct bt_gatt_service_val *)attr->user_data;

		LOG_DBG("service found: start=%u end=%u", attr->handle, svc->end_handle);

		atomic_clear(&disc_busy);
		if (disc_ctx.service_cb) {
			disc_ctx.service_cb(conn, attr->handle, svc->end_handle,
					    0, disc_ctx.user_data);
		}
		/* Return STOP — we only want the first matching service. */
		return BT_GATT_ITER_STOP;
	}

	case DISC_TYPE_CHAR: {
		/* attr->user_data points to struct bt_gatt_chrc which contains
		 * the characteristic UUID, its value_handle, and properties.
		 */
		const struct bt_gatt_chrc *chrc =
			(const struct bt_gatt_chrc *)attr->user_data;

		LOG_DBG("char found: decl_handle=%u value_handle=%u props=0x%02x",
			attr->handle, chrc->value_handle, chrc->properties);

		atomic_clear(&disc_busy);
		if (disc_ctx.char_cb) {
			disc_ctx.char_cb(conn, chrc->value_handle, chrc->properties,
					 0, disc_ctx.user_data);
		}
		return BT_GATT_ITER_STOP;
	}

	case DISC_TYPE_DESC: {
		/* For descriptors, attr->handle is the descriptor handle and
		 * attr->uuid is the descriptor type UUID (e.g. 0x2902 for CCC).
		 * Do NOT clear disc_busy here — let the caller decide whether
		 * to stop (by returning BT_GATT_ITER_STOP from the user cb or
		 * waiting for attr == NULL).
		 */
		LOG_DBG("desc found: handle=%u", attr->handle);

		if (disc_ctx.desc_cb) {
			disc_ctx.desc_cb(conn, attr->handle, attr->uuid,
					 0, disc_ctx.user_data);
		}
		/* Continue scanning — the user may want multiple descriptors. */
		return BT_GATT_ITER_CONTINUE;
	}
	}

	return BT_GATT_ITER_CONTINUE;
}

int ble_gatt_client_discover_service(struct bt_conn *conn,
				     const struct bt_uuid *uuid,
				     ble_gatt_client_service_found_cb_t cb,
				     void *user_data)
{
	if (conn == NULL || uuid == NULL || cb == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set(&disc_busy)) {
		return -EBUSY;
	}

	disc_ctx.type       = DISC_TYPE_SERVICE;
	disc_ctx.service_cb = cb;
	disc_ctx.user_data  = user_data;

	disc_params.uuid         = uuid;
	disc_params.func         = on_discover;
	disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	disc_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	disc_params.type         = BT_GATT_DISCOVER_PRIMARY;

	int err = bt_gatt_discover(conn, &disc_params);
	if (err) {
		atomic_clear(&disc_busy);
		LOG_ERR("bt_gatt_discover (service) failed (%d)", err);
	}

	return err;
}

int ble_gatt_client_discover_char(struct bt_conn *conn,
				  uint16_t start_handle,
				  uint16_t end_handle,
				  const struct bt_uuid *uuid,
				  ble_gatt_client_char_found_cb_t cb,
				  void *user_data)
{
	if (conn == NULL || uuid == NULL || cb == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set(&disc_busy)) {
		return -EBUSY;
	}

	disc_ctx.type     = DISC_TYPE_CHAR;
	disc_ctx.char_cb  = cb;
	disc_ctx.user_data = user_data;

	disc_params.uuid         = uuid;
	disc_params.func         = on_discover;
	disc_params.start_handle = start_handle;
	disc_params.end_handle   = end_handle;
	disc_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

	int err = bt_gatt_discover(conn, &disc_params);
	if (err) {
		atomic_clear(&disc_busy);
		LOG_ERR("bt_gatt_discover (char) failed (%d)", err);
	}

	return err;
}

int ble_gatt_client_discover_desc(struct bt_conn *conn,
				  uint16_t start_handle,
				  uint16_t end_handle,
				  ble_gatt_client_desc_found_cb_t cb,
				  void *user_data)
{
	if (conn == NULL || cb == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set(&disc_busy)) {
		return -EBUSY;
	}

	disc_ctx.type      = DISC_TYPE_DESC;
	disc_ctx.desc_cb   = cb;
	disc_ctx.user_data = user_data;

	/* No UUID filter for descriptor discovery — scan all descriptors in range. */
	disc_params.uuid         = NULL;
	disc_params.func         = on_discover;
	disc_params.start_handle = start_handle;
	disc_params.end_handle   = end_handle;
	disc_params.type         = BT_GATT_DISCOVER_DESCRIPTOR;

	int err = bt_gatt_discover(conn, &disc_params);
	if (err) {
		atomic_clear(&disc_busy);
		LOG_ERR("bt_gatt_discover (desc) failed (%d)", err);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * Read state
 * --------------------------------------------------------------------------
 */
static struct bt_gatt_read_params read_params;
static atomic_t                   read_busy;

static struct {
	ble_gatt_client_read_cb_t cb;
	void                     *user_data;
} read_ctx;

static uint8_t on_read(struct bt_conn *conn,
		       uint8_t err,
		       struct bt_gatt_read_params *params,
		       const void *data,
		       uint16_t length)
{
	ARG_UNUSED(params);

	atomic_clear(&read_busy);

	if (read_ctx.cb) {
		read_ctx.cb(conn, err, data, length, read_ctx.user_data);
	}

	return BT_GATT_ITER_STOP;
}

int ble_gatt_client_read(struct bt_conn *conn,
			 uint16_t handle,
			 ble_gatt_client_read_cb_t cb,
			 void *user_data)
{
	if (conn == NULL || handle == 0U || cb == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set(&read_busy)) {
		return -EBUSY;
	}

	read_ctx.cb        = cb;
	read_ctx.user_data = user_data;

	read_params.func                 = on_read;
	read_params.handle_count         = 1U;
	read_params.single.handle        = handle;
	read_params.single.offset        = 0U;

	int err = bt_gatt_read(conn, &read_params);
	if (err) {
		atomic_clear(&read_busy);
		LOG_ERR("bt_gatt_read failed (%d)", err);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * Write state
 * --------------------------------------------------------------------------
 */
static struct bt_gatt_write_params write_params;
static atomic_t                    write_busy;

static struct {
	ble_gatt_client_write_cb_t cb;
	void                      *user_data;
} write_ctx;

/* Write data staging buffer — bt_gatt_write_params.data must remain valid
 * until the callback fires.  244 bytes matches the maximum ATT payload.
 */
static uint8_t write_buf[244];

static void on_write(struct bt_conn *conn,
		     uint8_t err,
		     struct bt_gatt_write_params *params)
{
	ARG_UNUSED(params);

	atomic_clear(&write_busy);

	if (write_ctx.cb) {
		write_ctx.cb(conn, err, write_ctx.user_data);
	}
}

int ble_gatt_client_write(struct bt_conn *conn,
			  uint16_t handle,
			  const void *data,
			  uint16_t len,
			  ble_gatt_client_write_cb_t cb,
			  void *user_data)
{
	if (conn == NULL || handle == 0U || data == NULL || len == 0U) {
		return -EINVAL;
	}

	if (len > sizeof(write_buf)) {
		return -EMSGSIZE;
	}

	if (atomic_test_and_set(&write_busy)) {
		return -EBUSY;
	}

	/* Copy to static buffer so the caller's buffer can be freed after this
	 * function returns (the write is async).
	 */
	memcpy(write_buf, data, len);

	write_ctx.cb        = cb;
	write_ctx.user_data = user_data;

	write_params.func   = on_write;
	write_params.handle = handle;
	write_params.offset = 0U;
	write_params.data   = write_buf;
	write_params.length = len;

	int err = bt_gatt_write(conn, &write_params);
	if (err) {
		atomic_clear(&write_busy);
		LOG_ERR("bt_gatt_write failed (%d)", err);
	}

	return err;
}

int ble_gatt_client_write_no_rsp(struct bt_conn *conn,
				 uint16_t handle,
				 const void *data,
				 uint16_t len)
{
	if (conn == NULL || handle == 0U || data == NULL || len == 0U) {
		return -EINVAL;
	}

	/* bt_gatt_write_without_response_cb() is fire-and-forget.
	 * The NULL callback means no completion notification.
	 */
	int err = bt_gatt_write_without_response_cb(conn, handle, data, len,
						     false, NULL, NULL);
	if (err) {
		LOG_ERR("write_without_response failed (%d)", err);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * Subscription pool
 * --------------------------------------------------------------------------
 * bt_gatt_subscribe_params must remain valid (static storage) until
 * bt_gatt_unsubscribe() is called.  We maintain a fixed pool of slots.
 * --------------------------------------------------------------------------
 */
static struct {
	struct bt_gatt_subscribe_params params;
	ble_gatt_client_notify_cb_t     user_cb;
	void                           *user_data;
	bool                            in_use;
} subscriptions[CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS];

static struct k_mutex sub_mutex;

/* bt_gatt_subscribe_params.notify callback — dispatches to user cb. */
static uint8_t on_notify(struct bt_conn *conn,
			 struct bt_gatt_subscribe_params *params,
			 const void *data,
			 uint16_t length)
{
	/* Find the subscription slot by matching the params pointer. */
	for (int i = 0; i < CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS; i++) {
		if (&subscriptions[i].params == params) {
			if (data == NULL) {
				/* NULL data signals that the subscription was
				 * terminated by the peer (e.g. on disconnect).
				 */
				LOG_DBG("subscription %d terminated by peer", i);
				subscriptions[i].in_use = false;
				return BT_GATT_ITER_STOP;
			}

			if (subscriptions[i].user_cb != NULL) {
				return subscriptions[i].user_cb(
					conn, data, length,
					subscriptions[i].user_data);
			}
			return BT_GATT_ITER_CONTINUE;
		}
	}

	return BT_GATT_ITER_CONTINUE;
}

int ble_gatt_client_subscribe(struct bt_conn *conn,
			      uint16_t value_handle,
			      uint16_t ccc_handle,
			      ble_gatt_client_notify_cb_t cb,
			      void *user_data)
{
	if (conn == NULL || value_handle == 0U || ccc_handle == 0U || cb == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&sub_mutex, K_FOREVER);

	int slot = -1;

	for (int i = 0; i < CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS; i++) {
		if (!subscriptions[i].in_use) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		k_mutex_unlock(&sub_mutex);
		LOG_ERR("subscription pool full (%d slots)",
			CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS);
		return -ENOMEM;
	}

	subscriptions[slot].user_cb   = cb;
	subscriptions[slot].user_data = user_data;
	subscriptions[slot].in_use    = true;

	/* bt_gatt_subscribe_params fields:
	 *   notify        — notification callback (our dispatcher above)
	 *   value         — BT_GATT_CCC_NOTIFY or BT_GATT_CCC_INDICATE
	 *   value_handle  — characteristic value attribute handle
	 *   ccc_handle    — CCC descriptor handle (0x2902)
	 *
	 * bt_gatt_subscribe() writes BT_GATT_CCC_NOTIFY to ccc_handle and
	 * registers the notify callback with the BT stack.
	 */
	subscriptions[slot].params.notify       = on_notify;
	subscriptions[slot].params.value        = BT_GATT_CCC_NOTIFY;
	subscriptions[slot].params.value_handle = value_handle;
	subscriptions[slot].params.ccc_handle   = ccc_handle;

	k_mutex_unlock(&sub_mutex);

	int err = bt_gatt_subscribe(conn, &subscriptions[slot].params);
	if (err) {
		k_mutex_lock(&sub_mutex, K_FOREVER);
		subscriptions[slot].in_use = false;
		k_mutex_unlock(&sub_mutex);
		LOG_ERR("bt_gatt_subscribe failed (%d)", err);
	} else {
		LOG_INF("subscribed: value_handle=%u ccc_handle=%u (slot %d)",
			value_handle, ccc_handle, slot);
	}

	return err;
}

int ble_gatt_client_unsubscribe(struct bt_conn *conn, uint16_t value_handle)
{
	if (conn == NULL || value_handle == 0U) {
		return -EINVAL;
	}

	k_mutex_lock(&sub_mutex, K_FOREVER);

	for (int i = 0; i < CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS; i++) {
		if (subscriptions[i].in_use &&
		    subscriptions[i].params.value_handle == value_handle) {

			int err = bt_gatt_unsubscribe(conn, &subscriptions[i].params);

			subscriptions[i].in_use = false;
			k_mutex_unlock(&sub_mutex);

			if (err) {
				LOG_ERR("bt_gatt_unsubscribe failed (%d)", err);
			} else {
				LOG_INF("unsubscribed: value_handle=%u (slot %d)",
					value_handle, i);
			}

			return err;
		}
	}

	k_mutex_unlock(&sub_mutex);
	return -ENOENT;
}

/* --------------------------------------------------------------------------
 * Connection event callback — release subscriptions on disconnect
 * --------------------------------------------------------------------------
 * When a connection drops, all BT_GATT_SUBSCRIBE_PARAMS structs are
 * invalidated by the stack automatically.  Clear the in_use flags so
 * the slots can be reused after the next connection.
 * --------------------------------------------------------------------------
 */
static void on_conn_event(ble_conn_evt_t evt, struct bt_conn *conn,
			  void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);

	if (evt != BLE_CONN_EVT_DISCONNECTED) {
		return;
	}

	k_mutex_lock(&sub_mutex, K_FOREVER);

	for (int i = 0; i < CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS; i++) {
		subscriptions[i].in_use = false;
	}

	k_mutex_unlock(&sub_mutex);

	/* Also clear any pending operation flags since the connection is gone. */
	atomic_clear(&disc_busy);
	atomic_clear(&read_busy);
	atomic_clear(&write_busy);

	LOG_DBG("GATT client: connection lost, state cleared");
}

/* --------------------------------------------------------------------------
 * Module initialisation
 * --------------------------------------------------------------------------
 */
static int ble_gatt_client_init(void)
{
	k_mutex_init(&sub_mutex);

	int err = ble_conn_register_event_cb(on_conn_event, NULL);
	if (err) {
		LOG_ERR("ble_conn_register_event_cb failed (%d)", err);
		return err;
	}

	LOG_INF("GATT client initialised (max %d subscriptions)",
		CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS);

	return 0;
}

SYS_INIT(ble_gatt_client_init, APPLICATION,
	 CONFIG_PACKAGE_BLE_GATT_CLIENT_INIT_PRIORITY);
