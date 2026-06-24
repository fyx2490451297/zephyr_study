# CMakeLists.txt for package_ble_gatt_client
if(CONFIG_PACKAGE_BLE_GATT_CLIENT)
    target_sources(app PRIVATE src/ble_gatt_client.c)
    target_include_directories(app PUBLIC inc)
endif()
