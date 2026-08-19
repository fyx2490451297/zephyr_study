# CMakeLists.txt for package_lcd_ui
if(CONFIG_PACKAGE_LCD_UI)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE
        src/lcd_ui.c
        src/pipe_bender_ui.c
        src/img_assets/angle_gauge_icon.c
        src/img_assets/battery_charging_0p_25p.c
        src/img_assets/battery_charging_25p_90p.c
        src/img_assets/battery_charging_90p_100p.c
        src/img_assets/battery_charging_not.c
        src/img_assets/battery_discharging_0p_10p.c
        src/img_assets/battery_discharging_10p_25p.c
        src/img_assets/battery_discharging_25p_50p.c
        src/img_assets/battery_discharging_50p_90p.c
        src/img_assets/battery_discharging_90p_100p.c
        src/fonts/font_awesome_12.c
        src/fonts/font_icons_16.c
        src/fonts/font_bend_bold_italic_14.c
    )
endif()
