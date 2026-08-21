## 第一步：更新系统并安装系统依赖

首先需要确保系统是最新并且安装zephyr编译所需的底层工具(Cmake,Ninja和设备树编译器)

打开终端，运行以下命令：

```
sudo apt update
sudo apt upgrade

# 安装核心开发工具和依赖

```
sudo apt install -y git cmake ninja-build gperf ccache dfu-util \
  device-tree-compiler wget python3-dev python3-pip python3-venv \
  python3-tk python3-wheel xz-utils file make gcc
```

---

## 第二步：创建python虚拟环境

最新的Raspberry Pi OS默认限制了全局pip安装，需要创建python虚拟环境

打开终端，运行以下命令：

```
mkdir -p ~/nrf_dev
cd ~/nrf_dev
python3 -m venv ncs-dev
source ncs-dev/bin/activate
```

注意：每次基于nrf52833打开新终端准备开发zephyr时，都需要先运行`source ~/nrf-dev/ncs-dev/bin/activate`来激活环境
激活完之后 前面会出现（ncs-dev）

---

## 第三步：安装 `west` 并拉取 nRF Connect SDK 代码

```
# 1. 安装 west
pip3 install west

# 2. 初始化 SDK（v3.3.0）
mkdir ~/nrf_dev/ncs
cd ~/nrf_dev/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.3.0

# 3. 拉取所有相关的代码仓库
west update

# 4. 导出 Zephyr 环境并安装相关的 Python 依赖包

pip3 install -r zephyr/scripts/requirements.txt

pip3 install -r nrf/scripts/requirements.txt

pip3 install -r bootloader/mcuboot/scripts/requirements.txt

west zephyr-export
```

---

## 第四步：安装 J-Link 驱动与配置 USB 权限

1. **下载 J-Link 驱动** ：前往 [SEGGER J-Link 下载页](https://www.segger.com/downloads/jlink/)，下载 **J-Link Software and Documentation Pack** 中的 **Linux ARM64 DEB** 格式安装包。
2. **安装驱动文件** ：

   ```
   sudo dpkg -i JLink_Linux_V*.deb
   ```
3. **安装 Nordic udev 规则**

   ```
   cd ~
   git clone https://github.com/NordicSemiconductor/nrf-udev.git
   cd nrf-udev

   # 打包生成 .deb 安装文件
   dpkg-deb -b nrf-udev_1.0.1-all

   # 安装生成的驱动包
   sudo dpkg -i nrf-udev_1.0.1-all.deb

   # 刷新系统 udev 规则使其立即生效
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```

---

## 第五步：编译并烧录第一个蓝牙程序

编译经典的蓝牙串口透传（NUS）示例

```
# 确保在 Python 虚拟环境中，并进入蓝牙 UART 示例目录
cd ~/ncs/nrf/samples/bluetooth/peripheral_uart

# 编译工程 (nRF52833dk 在新版 SDK 中的设备树目标名称如下)
west build -b nrf52833dk/nrf52833

# 烧录代码到开发板
west flash --runner jlink
```

---
