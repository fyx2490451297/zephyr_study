# CMakeLists.txt for package_lvgl_demo
if(CONFIG_PACKAGE_LVGL_DEMO)
    target_sources(app PRIVATE src/lvgl_demo.c)
endif()
