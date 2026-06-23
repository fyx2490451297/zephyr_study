#ifndef MCU_TRANSPORT_H_
#define MCU_TRANSPORT_H_

#include <stddef.h>
#include <stdint.h>

typedef void (*mcu_transport_rx_callback_t)(const uint8_t *data, size_t len, void *user_data);

/**
 * @brief Register or replace RX callback.
 *
 * @param cb RX callback. Pass NULL to unregister callback.
 * @param user_data Opaque pointer passed back when cb is invoked.
 *
 * @return 0 on success, negative errno on failure.
 */
int mcu_transport_register_rx_callback(mcu_transport_rx_callback_t cb, void *user_data);

/**
 * @brief Send bytes through UART transport.
 *
 * @param data Byte buffer to send.
 * @param len Number of bytes to send.
 *
 * @return 0 on success, negative errno on failure.
 */
int mcu_transport_send(const uint8_t *data, size_t len);

#endif /* MCU_TRANSPORT_H_ */
