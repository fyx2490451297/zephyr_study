#ifndef BLE_CORE_H_
#define BLE_CORE_H_

#include <stdbool.h>

/**
 * @brief Check whether the Bluetooth stack has been successfully initialised.
 *
 * Other BLE packages call this to guard operations that require the stack to
 * be ready.  Under normal boot ordering (ble_core SYS_INIT priority < all
 * other BLE packages) this will always return true by the time any dependent
 * module's init function runs.
 *
 * @return true if bt_enable() completed successfully, false otherwise.
 */
bool ble_core_is_ready(void);

#endif /* BLE_CORE_H_ */
