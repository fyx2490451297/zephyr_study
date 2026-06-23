# CMakeLists.txt for package_ble_adv_demo
if(CONFIG_PACKAGE_BLE_ADV_DEMO)
    target_sources(app PRIVATE src/ble_adv_demo.c)
endif()
