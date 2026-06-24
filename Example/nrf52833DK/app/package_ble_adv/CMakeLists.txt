# CMakeLists.txt for package_ble_adv
if(CONFIG_PACKAGE_BLE_ADV)
    target_sources(app PRIVATE src/ble_adv.c)
    target_include_directories(app PUBLIC inc)
endif()
