cmake_minimum_required(VERSION 3.20.0)

# 设置 Zephyr SDK 的路径
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 定义项目名称
project(STM32F407)

# 载入底层硬件与功能模块
add_subdirectory(module)

# 载入顶层业务应用模块
add_subdirectory(app)