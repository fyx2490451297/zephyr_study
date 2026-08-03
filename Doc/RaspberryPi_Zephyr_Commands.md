# Raspberry Pi 极简 Linux 命令指南（Zephyr 开发专用）

在 Raspberry Pi 上进行 Zephyr RTOS 开发时，完全可以将其视为一台普通的 Linux 电脑。由于 Zephyr 提供了高度集成的构建工具，开发者无需深入了解 Linux 底层机制。掌握以下四个方面的“极简命令包”即可顺利开展日常开发。

## 1. 基础生存：文件与目录
在终端中进行日常导航与文件管理的必备命令：

| 命令 | 作用 | 常见用法示例 |
|---|---|---|
| `pwd` | 查看当前绝对路径 | 迷路时确认当前所在位置 |
| `ls` | 列出当前目录内容 | `ls -l`（详细信息），`ls -a`（包含隐藏文件） |
| `cd` | 切换目录 | `cd zephyrproject`（进入目录），`cd ..`（返回上一级） |
| `mkdir` | 创建新文件夹 | `mkdir my_project` |
| `rm` | 删除文件或目录 | `rm file.c`（删文件），`rm -rf folder`（强制删目录，**慎用**） |

## 2. 软件与环境安装
在配置 CMake、Ninja 或 Zephyr SDK 时，常需要管理员权限和包管理器：

* **`sudo`**：获取临时管理员权限，用于系统配置、访问硬件节点或安装软件。
* **`sudo apt update`**：更新系统的软件源列表（安装新软件前的首要步骤）。
* **`sudo apt install <包名>`**：安装所需工具。
  * *示例*：`sudo apt install git cmake ninja-build`

## 3. 硬件交互：烧录与串口调试
确保树莓派能正确识别开发板和调试器（如 J-Link 或 ST-Link）：

* **`lsusb`**：列出当前连接的 USB 设备，验证调试硬件是否被成功识别。
* **`dmesg | tail`**：查看系统最新日志。插入开发板 USB 后运行，可快速确认分配的串口节点（通常为 `/dev/ttyACM0` 或 `/dev/ttyUSB0`）。
* **`picocom`**：轻量级串口调试工具（需先 `sudo apt install picocom`）。
  * *示例*：`picocom -b 115200 /dev/ttyACM0`（查看板子运行日志，退出按 `Ctrl+A` 后按 `Ctrl+X`）。

## 4. Zephyr 核心构建流：West
Zephyr 的元工具 `west` 接管了底层 `cmake` 和 `make` 的繁琐工作：

* **`west update`**：拉取或同步 Zephyr 核心代码库及各个芯片平台的依赖组件。
* **`west build`**：编译应用。为了防止旧缓存干扰，通常会添加强制清理参数。
  * *示例*：`west build -p always -b nucleo_f429zi`（指定目标 MCU 开发板进行全新编译）。
* **`west flash`**：将编译好的固件通过调试器一键烧录进目标芯片中，也是验证 bootloader 和 OTA 本地运行效果的最后一步。
