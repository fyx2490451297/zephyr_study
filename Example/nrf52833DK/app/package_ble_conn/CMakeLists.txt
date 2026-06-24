# CMakeLists.txt for package_ble_conn
if(CONFIG_PACKAGE_BLE_CONN)
    target_sources(app PRIVATE src/ble_conn.c)
    target_include_directories(app PUBLIC inc)
endif()
