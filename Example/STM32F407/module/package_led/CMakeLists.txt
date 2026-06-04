# 不再使用 zephyr_library() 把它变成独立的静态库
# 而是直接把源码“挂载”到 Zephyr 默认的 app（主应用程序）目标上
if(CONFIG_PACKAGE_LED)
    target_sources(app PRIVATE src/app_led.c)
endif()

# 依然保持头文件对全局可见
zephyr_include_directories(inc)