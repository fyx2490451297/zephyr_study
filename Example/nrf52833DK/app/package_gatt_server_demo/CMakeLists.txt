# CMakeLists.txt for package_gatt_server_demo
if(CONFIG_PACKAGE_GATT_SERVER_DEMO)
    target_sources(app PRIVATE src/gatt_server_demo.c)
endif()
