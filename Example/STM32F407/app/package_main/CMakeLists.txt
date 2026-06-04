# 将业务主代码编译进 Zephyr 的 app 目标
target_sources(app PRIVATE src/main.c)

# 暴露 package_main 的专属头文件目录
zephyr_include_directories(inc)