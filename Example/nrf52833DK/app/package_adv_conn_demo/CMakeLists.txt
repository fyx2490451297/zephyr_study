# CMakeLists.txt for package_adv_conn_demo
if(CONFIG_PACKAGE_ADV_CONN_DEMO)
    target_sources(app PRIVATE src/ble_adv_conn_demo.c)
endif()
