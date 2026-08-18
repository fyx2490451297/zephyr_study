# CMakeLists.txt for package_lcd_backlight
if(CONFIG_PACKAGE_LCD_BACKLIGHT)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE src/lcd_backlight.c)
endif()
