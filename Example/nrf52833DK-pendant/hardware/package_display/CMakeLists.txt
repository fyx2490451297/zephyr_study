# CMakeLists.txt for package_display
if(CONFIG_PACKAGE_DISPLAY)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE src/display.c)
endif()
