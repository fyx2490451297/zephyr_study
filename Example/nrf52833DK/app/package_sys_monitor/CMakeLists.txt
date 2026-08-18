# CMakeLists.txt for package_sys_monitor
if(CONFIG_PACKAGE_SYS_MONITOR)
    zephyr_include_directories(inc)
    target_sources(app PRIVATE
        src/sys_monitor.c
    )
endif()
