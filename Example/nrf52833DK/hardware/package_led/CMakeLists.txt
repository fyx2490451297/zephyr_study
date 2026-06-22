# CMakeLists.txt for package_led
if(CONFIG_PACKAGE_LED)
    zephyr_library_named(package_led)
    zephyr_library_sources(src/led.c)
    zephyr_include_directories(inc)
endif()