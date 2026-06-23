# CMakeLists.txt for package_ble_scan_conn_demo
if(CONFIG_PACKAGE_BLE_SCAN_CONN_DEMO)
    target_sources(app PRIVATE src/ble_scan_conn_demo.c)
endif()
