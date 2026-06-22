# CMakeLists.txt for package_ble_demo
if(CONFIG_PACKAGE_BLE_DEMO)
    target_sources(app PRIVATE src/ble_demo.c)
endif()
