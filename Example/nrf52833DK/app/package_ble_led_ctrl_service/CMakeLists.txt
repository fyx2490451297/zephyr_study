# CMakeLists.txt for package_ble_led_ctrl_service
if(CONFIG_PACKAGE_BLE_LED_CTRL_SERVICE)
    target_sources(app PRIVATE src/ble_led_ctrl_service.c)
    target_include_directories(app PUBLIC inc)
endif()
