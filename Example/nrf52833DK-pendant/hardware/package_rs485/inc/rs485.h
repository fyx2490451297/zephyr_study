#ifndef RS485_H_
#define RS485_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief RS485 RX callback signature.
 *
 * Invoked from the RS485 RX thread context whenever new bytes are pulled
 * out of the UART1 RX ring buffer while the bus is in receive mode.
 *
 * @param data Pointer to received byte buffer (valid only during callback).
 * @param len Number of valid bytes in @p data.
 * @param user_data Opaque pointer registered via rs485_register_rx_callback().
 */
typedef void (*rs485_rx_callback_t)(const uint8_t *data, size_t len, void *user_data);

/**
 * @brief Register or replace the RS485 RX callback.
 *
 * @param cb RX callback. Pass NULL to unregister callback.
 * @param user_data Opaque pointer passed back when cb is invoked.
 *
 * @return 0 on success, negative errno on failure.
 */
int rs485_register_rx_callback(rs485_rx_callback_t cb, void *user_data);

/**
 * @brief Send a frame over the RS485 bus.
 *
 * Drives the transceiver DE/RE pin high before transmission, feeds the
 * bytes through UART1, waits for the UART shift register to fully drain,
 * then returns the transceiver to receive mode. Blocks the calling thread
 * until the frame has been sent (or the send times out).
 *
 * @param data Byte buffer to send.
 * @param len Number of bytes to send. Must not exceed
 *            CONFIG_PACKAGE_RS485_TX_BUFFER_SIZE.
 *
 * @return 0 on success, negative errno on failure (-EMSGSIZE if len is too
 *         large, -ETIMEDOUT if the line failed to drain in time).
 */
int rs485_send(const uint8_t *data, size_t len);

#endif /* RS485_H_ */
