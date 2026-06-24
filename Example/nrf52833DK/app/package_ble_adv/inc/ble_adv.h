#ifndef BLE_ADV_H_
#define BLE_ADV_H_

#include <stdbool.h>

/**
 * @brief Start connectable BLE advertising.
 *
 * Uses the preset advertising parameters: connectable fast interval
 * (BT_LE_ADV_CONN_FAST_1), device name in Scan Response data.
 * Calling this while advertising is already active returns -EALREADY.
 *
 * @return 0 on success, negative errno on failure.
 */
int ble_adv_start(void);

/**
 * @brief Stop BLE advertising.
 *
 * No-op if advertising is not currently active.
 *
 * @return 0 on success, negative errno on failure.
 */
int ble_adv_stop(void);

/**
 * @brief Check whether advertising is currently active.
 *
 * @return true if advertising, false otherwise.
 */
bool ble_adv_is_active(void);

#endif /* BLE_ADV_H_ */
