# CMakeLists.txt for package_rs485
if(CONFIG_PACKAGE_RS485)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE src/rs485.c)
endif()
