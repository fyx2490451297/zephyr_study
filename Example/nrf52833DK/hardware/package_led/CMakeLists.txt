# CMakeLists.txt for package_led
if(CONFIG_PACKAGE_LED)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE src/led.c)
endif()