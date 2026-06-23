# CMakeLists.txt for package_mcu_transport
if(CONFIG_PACKAGE_MCU_TRANSPORT)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE src/mcu_transport.c)
endif()
