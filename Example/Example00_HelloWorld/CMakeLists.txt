# 指定最低 CMake 版本要求
cmake_minimum_required(VERSION 3.20.0)

# 寻找 Zephyr 系统 (自动引入所有的编译工具链和内核库)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 定义项目名称
project(Study00_HelloWorld)

# 添加你的源文件到 'app' 目标中
target_sources(app PRIVATE src/main.c)
