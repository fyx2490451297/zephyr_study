#ifndef BLE_GATT_OTA_SERVICE_H_
#define BLE_GATT_OTA_SERVICE_H_

#include <zephyr/bluetooth/uuid.h>

/* --------------------------------------------------------------------------
 * BLE OTA GATT Service — UUID definitions
 * --------------------------------------------------------------------------
 * Custom 128-bit UUIDs for the OTA service and its characteristics.
 * The same UUID values must be programmed into the OTA host application
 * (phone app / PC tool) that performs the firmware update.
 *
 * Service layout:
 *
 *   OTA Service          (UUID: BLE_GATT_OTA_SVC_UUID_VAL)
 *   ├── Control Point    (Write + Notify) — OTA commands (START/END/ABORT)
 *   │   CCC descriptor  (notify enabled by Central for command ACK)
 *   ├── Data             (Write Without Response) — firmware chunk relay
 *   └── Status           (Notify) — MCU response relay to Central
 *       CCC descriptor  (notify enabled by Central for MCU responses)
 *
 * Data flow:
 *
 *   [BLE Central]                [nRF52833]              [Host MCU]
 *      Write Control Point  →  forward via UART  →  MCU command handler
 *      Write Data           →  forward via UART  →  MCU OTA receiver
 *                           ←  MCU UART response ←  ACK / NAK / progress
 *      Notify Status        ←  Status notify     ←
 *
 * The MCU protocol framing ([0xAA][0x55][CMD][SEQ][LEN][payload][CRC])
 * is transparent to this module; raw bytes are forwarded unchanged.
 * --------------------------------------------------------------------------
 */

/* OTA Service UUID: AABBCCDD-0001-1000-8000-00805F9B34F0 */
#define BLE_GATT_OTA_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0xAABBCCDD, 0x0001, 0x1000, 0x8000, 0x00805F9B34F0)

/* Control Point Characteristic UUID: AABBCCDD-0001-1000-8000-00805F9B34F1 */
#define BLE_GATT_OTA_CTRL_PT_UUID_VAL \
	BT_UUID_128_ENCODE(0xAABBCCDD, 0x0001, 0x1000, 0x8000, 0x00805F9B34F1)

/* Data Characteristic UUID: AABBCCDD-0001-1000-8000-00805F9B34F2 */
#define BLE_GATT_OTA_DATA_UUID_VAL \
	BT_UUID_128_ENCODE(0xAABBCCDD, 0x0001, 0x1000, 0x8000, 0x00805F9B34F2)

/* Status Characteristic UUID: AABBCCDD-0001-1000-8000-00805F9B34F3 */
#define BLE_GATT_OTA_STATUS_UUID_VAL \
	BT_UUID_128_ENCODE(0xAABBCCDD, 0x0001, 0x1000, 0x8000, 0x00805F9B34F3)

#endif /* BLE_GATT_OTA_SERVICE_H_ */
