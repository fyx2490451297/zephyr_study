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
