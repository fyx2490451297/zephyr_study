## 项目结构设计


STM32F407_Project/
├── CMakeLists.txt        # 工程主构建脚本 (极其精简)
├── prj.conf              # 系统核心配置 (Zephyr 原生配置)
├── Kconfig               # 自定义 App 级 Kconfig 树 (层层包含模块配置)
├── app.overlay           # 针对本工程的设备树覆写文件
│
├── include/              # 全局头文件 (对外暴露的系统级接口)
│   └── system_config.h
│
├── src/                  # 顶层业务逻辑
│   └── main.c            # 纯粹的业务状态机，无底层初始化代码
│
└── module/               # 独立功能模块 (组件库)
    ├── CMakeLists.txt    # 模块路由脚本 (用于添加各个 package)
    │
    ├── package_led/      # LED 模块
    │   ├── CMakeLists.txt# 声明静态库并按需编译
    │   ├── Kconfig       # LED 模块特有配置 (如闪烁频率)
    │   ├── inc/          # 对外暴露的接口 (app_led.h)
    │   └── src/          # 具体实现，使用 SYS_INIT 自动初始化
    │
    ├── package_fs/       # 文件系统模块 (LittleFS)
    │   ├── CMakeLists.txt
    │   ├── Kconfig
    │   ├── inc/
    │   └── src/
    │
    └── package_sensor/   # 预留的其他传感器模块
        ├── ...
