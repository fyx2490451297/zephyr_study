# CMakeLists.txt for package_led_ctrl
if(CONFIG_PACKAGE_LED_CTRL)
    zephyr_library_named(package_led_ctrl)
    zephyr_library_sources(src/led_ctrl.c)
    target_sources(app PRIVATE src/led_ctrl.c)
endif()
