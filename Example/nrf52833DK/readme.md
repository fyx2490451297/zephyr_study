# Project guidline

---

## Build step

Active zephyr example

```
source /home/ethan/nrf-dev/ncs/zephyr/zephyr-env.sh
```

Active python west venv

```
source /home/ethan/.venv/bin/activate
```

Run west build cmd

```
west build -p always -b nrf52833dk/nrf52833
```

Flash to MCU

```
west flash --runner jlink
```

---

## BLE study step

Step1. BLE boardcast example

Step2. BLE scan/connection

Step3. BLE GATT read/write service

---
## BLE OTA design

Project layout
 app/
 ├── package_ble_core/              # bt_enable，全局初始化
 ├── package_ble_adv/               # 广播管理
 ├── package_ble_conn/              # 连接管理
 ├── package_ble_gatt_service/      # 通用的GATT service
 ├── package_ble_gatt_ota_service/  # OTA GATT Service
 ├── package_ble_gatt_client/       # GATT Client
 └── package_mcu_transport/         # BLE MCU 物理传输层(UART)

### BLE MCU protocol design(Not need to implement in BLE part)

	BLE-->MCU Forward frame
		[Sync Head 1][Sync Head 2][CMD][SEQ][Length][Payload][CRC-16 MSB][CRC-16 LSB]

		1. Sync Head 1: 0xAA	1byte
		2. Sync Head 2: 0x55	1byte
		3. CMD:         0x00 ~ 0xFE	1byte (0xFF reserved)
		4. SEQ:         0x00 ~ 0xFE	1byte (0xFF reserved, rolls over 0xFE -> 0x00)
		5. Length:      0x00 ~ 0xFE	1byte (0xFF reserved, max payload 254 bytes)
		6. Payload:     N bytes
		7. CRC-16 MSB:  0x00 ~ 0xFF	1byte
		8. CRC-16 LSB:  0x00 ~ 0xFF	1byte

	BLE<--MCU Backward frame
		[Sync Head 1][Sync Head 2][CMD][SEQ][Length][Payload][CRC-16 MSB][CRC-16 LSB]

		1. Sync Head 1: 0x55	1byte
		2. Sync Head 2: 0xAA	1byte
		3. CMD:         0x00 ~ 0xFE	1byte (Echo the CMD from the received Forward frame)
		4. SEQ:         0x00 ~ 0xFE	1byte (Echo the SEQ from the received Forward frame)
		5. Length:      0x00 ~ 0xFE	1byte (0xFF reserved)
		6. Payload:     N bytes
		7. CRC-16 MSB:  0x00 ~ 0xFF	1byte
		8. CRC-16 LSB:  0x00 ~ 0xFF	1byte

	ACK/NAK mechanism

		ACK frame (MCU --> BLE, no payload):
			[0x55][0xAA][CMD][SEQ][0x00][CRC-16 MSB][CRC-16 LSB]
			- CMD and SEQ echo the Forward frame being acknowledged
			- Length = 0x00 means no payload

		NAK frame (MCU --> BLE, 1 byte error code):
			[0x55][0xAA][CMD][SEQ][0x01][ERR_CODE][CRC-16 MSB][CRC-16 LSB]
			- CMD and SEQ echo the Forward frame being rejected
			- Length = 0x01, Payload = ERR_CODE (1 byte)

		ERR_CODE definition:
			0x01  CRC error       payload CRC mismatch
			0x02  SEQ error       unexpected sequence number (packet loss detected)
			0x03  LEN error       length field out of range
			0x04  CMD unknown     unrecognised CMD value
			0x05  BUSY            receiver not ready (e.g. Flash write in progress)
			0x06  TIMEOUT         sender did not receive ACK within timeout window

	Retransmission rules
		- Sender must wait for ACK before sending the next frame
		- If NAK is received, retransmit the same frame (same SEQ) up to 3 times
		- If no ACK/NAK is received within 500 ms, treat as timeout and retransmit
		- After 3 consecutive failures, abort and report error to upper layer

	SEQ rules
		- SEQ increments by 1 for each new Forward frame: 0x00 -> 0x01 -> ... -> 0xFE -> 0x00
		- Retransmitted frames keep the same SEQ as the original
		- Receiver detects duplicate frames by comparing SEQ with the last accepted SEQ