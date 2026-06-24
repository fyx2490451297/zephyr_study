#ifndef BLE_LED_CTRL_SERVICE_H_
#define BLE_LED_CTRL_SERVICE_H_

#include <zephyr/bluetooth/uuid.h>

/* --------------------------------------------------------------------------
 * BLE LED Control Service — UUID definitions
 * --------------------------------------------------------------------------
 * Custom 128-bit UUIDs.  Programme the same values into the phone/app that
 * needs to discover and control the LEDs.
 *
 * Service layout:
 *
 *   LED Control Service   (UUID: BLE_LED_CTRL_SVC_UUID_VAL)
 *   ├── LED3 Control      (Read + Write + Notify)
 *   │   ├── Value         1 byte: 0x00 = OFF, 0x01 = ON
 *   │   └── CCC           Enable notify to receive state change events
 *   └── LED4 Control      (Read + Write + Notify)
 *       ├── Value         1 byte: 0x00 = OFF, 0x01 = ON
 *       └── CCC           Enable notify to receive state change events
 *
 * App interaction:
 *   1. Connect to the device.
 *   2. Discover service UUID BLE_LED_CTRL_SVC_UUID_VAL.
 *   3. Write 0x01 to LED3 characteristic → LED3 turns ON.
 *   4. Write 0x00 to LED3 characteristic → LED3 turns OFF.
 *   5. Read LED4 characteristic → get current LED4 state.
 *   6. Write 0x0001 to LED3/LED4 CCC → subscribe to state notifications.
 * --------------------------------------------------------------------------
 */

/* LED Control Service UUID: BBCCDDEE-0002-1000-8000-00805F9B34F0 */
#define BLE_LED_CTRL_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0xBBCCDDEE, 0x0002, 0x1000, 0x8000, 0x00805F9B34F0)

/* LED3 Control Characteristic UUID: BBCCDDEE-0002-1000-8000-00805F9B34F1 */
#define BLE_LED_CTRL_LED3_UUID_VAL \
	BT_UUID_128_ENCODE(0xBBCCDDEE, 0x0002, 0x1000, 0x8000, 0x00805F9B34F1)

/* LED4 Control Characteristic UUID: BBCCDDEE-0002-1000-8000-00805F9B34F2 */
#define BLE_LED_CTRL_LED4_UUID_VAL \
	BT_UUID_128_ENCODE(0xBBCCDDEE, 0x0002, 0x1000, 0x8000, 0x00805F9B34F2)

/**
 * @brief Update the LED state from outside the GATT service and notify
 *        any subscribed BLE Central.
 *
 * Call this whenever LED3 or LED4 state changes from a non-BLE source
 * (e.g. a button press) so the Central's UI stays in sync.
 *
 * @param led3_state New LED3 state: 0 = OFF, 1 = ON.  Pass -1 to skip.
 * @param led4_state New LED4 state: 0 = OFF, 1 = ON.  Pass -1 to skip.
 */
void ble_led_ctrl_service_notify(int led3_state, int led4_state);

#endif /* BLE_LED_CTRL_SERVICE_H_ */
