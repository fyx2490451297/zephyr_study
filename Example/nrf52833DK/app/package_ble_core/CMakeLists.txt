# CMakeLists.txt for package_ble_core
if(CONFIG_PACKAGE_BLE_CORE)
    target_sources(app PRIVATE src/ble_core.c)
    target_include_directories(app PUBLIC inc)
endif()
