# CMakeLists.txt for package_led_ctrl
if(CONFIG_PACKAGE_LED_CTRL)
    target_sources(app PRIVATE src/led_ctrl.c)
endif()
