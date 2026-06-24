# CMakeLists.txt for package_ble_gatt_ota_service
if(CONFIG_PACKAGE_BLE_GATT_OTA_SERVICE)
    target_sources(app PRIVATE src/ble_gatt_ota_service.c)
    target_include_directories(app PUBLIC inc)
endif()
