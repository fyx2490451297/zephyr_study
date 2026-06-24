#ifndef BLE_GATT_CLIENT_H_
#define BLE_GATT_CLIENT_H_

#include <stdint.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/* --------------------------------------------------------------------------
 * BLE GATT Client
 * --------------------------------------------------------------------------
 * Provides a simplified callback-based API for performing GATT client
 * operations when the nRF52833 acts in the Central (initiator) role.
 *
 * Typical discovery flow:
 *   1. ble_gatt_client_discover_service()  — find service, get handle range
 *   2. ble_gatt_client_discover_char()     — find characteristic value handle
 *   3. ble_gatt_client_read() / write()    — access the characteristic
 *   4. ble_gatt_client_subscribe()         — enable notifications (needs
 *      the CCC descriptor handle, found via ble_gatt_client_discover_desc)
 *
 * Concurrency note:
 *   Only one discover/read/write operation may be in flight at a time.
 *   The caller must wait for the callback before issuing a new request.
 *   Attempting to start a second operation while one is pending returns
 *   -EBUSY.  Multiple subscriptions are allowed concurrently (up to
 *   CONFIG_PACKAGE_BLE_GATT_CLIENT_MAX_SUBSCRIPTIONS).
 * --------------------------------------------------------------------------
 */

/* --------------------------------------------------------------------------
 * Callback types
 * --------------------------------------------------------------------------
 */

/**
 * @brief Service discovery result callback.
 *
 * @param conn         The connection the discovery was performed on.
 * @param start_handle First attribute handle of the discovered service,
 *                     or 0 if the service was not found.
 * @param end_handle   Last attribute handle of the service (inclusive),
 *                     or 0 if not found.
 * @param err          0 on success, negative errno on failure.
 * @param user_data    Opaque pointer supplied at discover call time.
 */
typedef void (*ble_gatt_client_service_found_cb_t)(struct bt_conn *conn,
						   uint16_t start_handle,
						   uint16_t end_handle,
						   int err,
						   void *user_data);

/**
 * @brief Characteristic discovery result callback.
 *
 * @param conn         The connection the discovery was performed on.
 * @param value_handle ATT handle of the characteristic value attribute,
 *                     or 0 if the characteristic was not found.
 * @param props        Characteristic properties bitmask (BT_GATT_CHRC_*),
 *                     or 0 if not found.
 * @param err          0 on success, negative errno on failure.
 * @param user_data    Opaque pointer supplied at discover call time.
 */
typedef void (*ble_gatt_client_char_found_cb_t)(struct bt_conn *conn,
						uint16_t value_handle,
						uint8_t props,
						int err,
						void *user_data);

/**
 * @brief Descriptor discovery result callback.
 *
 * Called once for each discovered descriptor; called with handle = 0 when
 * the scan is complete (no more descriptors in the requested range).
 *
 * @param conn         The connection the discovery was performed on.
 * @param handle       ATT handle of the discovered descriptor, or 0 if done.
 * @param uuid         UUID of the descriptor, or NULL if done.
 * @param err          0 on success, negative errno on failure.
 * @param user_data    Opaque pointer supplied at discover call time.
 */
typedef void (*ble_gatt_client_desc_found_cb_t)(struct bt_conn *conn,
						uint16_t handle,
						const struct bt_uuid *uuid,
						int err,
						void *user_data);

/**
 * @brief Characteristic read result callback.
 *
 * @param conn      The connection the read was performed on.
 * @param err       ATT error code (0 = success, BT_ATT_ERR_* on failure).
 * @param data      Pointer to the received value bytes, or NULL on error.
 * @param len       Number of bytes in data.
 * @param user_data Opaque pointer supplied at read call time.
 */
typedef void (*ble_gatt_client_read_cb_t)(struct bt_conn *conn,
					  uint8_t err,
					  const void *data,
					  uint16_t len,
					  void *user_data);

/**
 * @brief Characteristic write result callback.
 *
 * @param conn      The connection the write was performed on.
 * @param err       ATT error code (0 = success, BT_ATT_ERR_* on failure).
 * @param user_data Opaque pointer supplied at write call time.
 */
typedef void (*ble_gatt_client_write_cb_t)(struct bt_conn *conn,
					   uint8_t err,
					   void *user_data);

/**
 * @brief Incoming notification callback for a subscribed characteristic.
 *
 * @param conn      The connection that sent the notification.
 * @param data      Notification payload bytes, or NULL when the subscription
 *                  is terminated by the peer.
 * @param len       Number of bytes in data.
 * @param user_data Opaque pointer supplied at subscribe call time.
 *
 * @return BT_GATT_ITER_CONTINUE to keep the subscription active,
 *         BT_GATT_ITER_STOP to auto-unsubscribe after this notification.
 */
typedef uint8_t (*ble_gatt_client_notify_cb_t)(struct bt_conn *conn,
					       const void *data,
					       uint16_t len,
					       void *user_data);

/* --------------------------------------------------------------------------
 * Discovery API
 * --------------------------------------------------------------------------
 */

/**
 * @brief Discover a primary service by UUID.
 *
 * Searches the remote GATT server for a primary service with the given UUID.
 * Reports the service's start and end handles via the callback so the caller
 * can subsequently discover characteristics within that range.
 *
 * @param conn      Active connection to the remote GATT server.
 * @param uuid      UUID of the service to find.
 * @param cb        Callback invoked once with the result.
 * @param user_data Opaque pointer passed to cb.
 *
 * @return 0 if the discovery was queued, -EBUSY if another discovery is in
 *         progress, or a negative errno on other failures.
 */
int ble_gatt_client_discover_service(struct bt_conn *conn,
				     const struct bt_uuid *uuid,
				     ble_gatt_client_service_found_cb_t cb,
				     void *user_data);

/**
 * @brief Discover a characteristic by UUID within a service handle range.
 *
 * @param conn         Active connection.
 * @param start_handle First handle of the search range (service start_handle).
 * @param end_handle   Last handle of the search range (service end_handle).
 * @param uuid         UUID of the characteristic to find.
 * @param cb           Callback invoked once with the result.
 * @param user_data    Opaque pointer passed to cb.
 *
 * @return 0 on success, -EBUSY if discovery is in progress, or negative errno.
 */
int ble_gatt_client_discover_char(struct bt_conn *conn,
				  uint16_t start_handle,
				  uint16_t end_handle,
				  const struct bt_uuid *uuid,
				  ble_gatt_client_char_found_cb_t cb,
				  void *user_data);

/**
 * @brief Discover descriptors within a handle range.
 *
 * Useful for finding the CCC descriptor handle (UUID 0x2902) needed to
 * enable notifications before calling ble_gatt_client_subscribe().
 *
 * @param conn         Active connection.
 * @param start_handle Start of descriptor search range.
 * @param end_handle   End of descriptor search range (inclusive).
 * @param cb           Callback invoked for each descriptor; called with
 *                     handle = 0 when the scan is complete.
 * @param user_data    Opaque pointer passed to cb.
 *
 * @return 0 on success, -EBUSY if discovery is in progress, or negative errno.
 */
int ble_gatt_client_discover_desc(struct bt_conn *conn,
				  uint16_t start_handle,
				  uint16_t end_handle,
				  ble_gatt_client_desc_found_cb_t cb,
				  void *user_data);

/* --------------------------------------------------------------------------
 * Read / Write API
 * --------------------------------------------------------------------------
 */

/**
 * @brief Read a characteristic value by handle.
 *
 * Issues an ATT_READ_REQ.  For values larger than ATT_MTU − 1 bytes,
 * the stack automatically issues ATT_READ_BLOB_REQ to reassemble the full
 * value before invoking the callback.
 *
 * @param conn      Active connection.
 * @param handle    ATT handle of the characteristic value attribute.
 * @param cb        Callback invoked with the read result.
 * @param user_data Opaque pointer passed to cb.
 *
 * @return 0 on success, -EBUSY if another read is in progress, or negative errno.
 */
int ble_gatt_client_read(struct bt_conn *conn,
			 uint16_t handle,
			 ble_gatt_client_read_cb_t cb,
			 void *user_data);

/**
 * @brief Write a characteristic value with ATT response.
 *
 * Issues an ATT_WRITE_REQ and waits for ATT_WRITE_RSP before invoking cb.
 *
 * @param conn      Active connection.
 * @param handle    ATT handle of the characteristic value attribute.
 * @param data      Bytes to write.
 * @param len       Number of bytes to write.
 * @param cb        Callback invoked with the write result (NULL to skip).
 * @param user_data Opaque pointer passed to cb.
 *
 * @return 0 on success, -EBUSY if another write is in progress, or negative errno.
 */
int ble_gatt_client_write(struct bt_conn *conn,
			  uint16_t handle,
			  const void *data,
			  uint16_t len,
			  ble_gatt_client_write_cb_t cb,
			  void *user_data);

/**
 * @brief Write a characteristic value without ATT response.
 *
 * Issues an ATT_WRITE_CMD.  The remote server does not send a response;
 * this is a fire-and-forget operation suitable for high-throughput streams.
 *
 * @param conn   Active connection.
 * @param handle ATT handle of the characteristic value attribute.
 * @param data   Bytes to write.
 * @param len    Number of bytes to write.
 *
 * @return 0 on success, or a negative errno on failure.
 */
int ble_gatt_client_write_no_rsp(struct bt_conn *conn,
				 uint16_t handle,
				 const void *data,
				 uint16_t len);

/* --------------------------------------------------------------------------
 * Notification subscription API
 * --------------------------------------------------------------------------
 */

/**
 * @brief Subscribe to notifications from a remote characteristic.
 *
 * Writes 0x0001 (BT_GATT_CCC_NOTIFY) to the CCC descriptor at ccc_handle
 * and registers a callback for incoming ATT_HANDLE_VALUE_NTF PDUs.
 *
 * The ccc_handle must be obtained via ble_gatt_client_discover_desc()
 * before calling this function.
 *
 * @param conn         Active connection.
 * @param value_handle ATT handle of the characteristic value attribute.
 * @param ccc_handle   ATT handle of the CCC descriptor (UUID 0x2902).
 * @param cb           Called for each incoming notification.
 * @param user_data    Opaque pointer passed to cb.
 *
 * @return 0 on success, -ENOMEM if the subscription pool is full, or
 *         a negative errno on failure.
 */
int ble_gatt_client_subscribe(struct bt_conn *conn,
			      uint16_t value_handle,
			      uint16_t ccc_handle,
			      ble_gatt_client_notify_cb_t cb,
			      void *user_data);

/**
 * @brief Unsubscribe from notifications for a characteristic.
 *
 * Writes 0x0000 to the CCC descriptor and frees the subscription slot.
 *
 * @param conn         Active connection.
 * @param value_handle ATT handle of the characteristic value attribute
 *                     (used to identify the subscription to remove).
 *
 * @return 0 on success, -ENOENT if the subscription was not found, or
 *         a negative errno on failure.
 */
int ble_gatt_client_unsubscribe(struct bt_conn *conn, uint16_t value_handle);

#endif /* BLE_GATT_CLIENT_H_ */
