#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>

#include <zephyr/logging/log.h>

#include <string.h>

LOG_MODULE_REGISTER(blinky_demo, LOG_LEVEL_DBG);

/* 从设备树获取 blue_led 别名对应的硬件信息 */
#define BLUE_LED    DT_ALIAS(led0)
#define RED_LED     DT_ALIAS(led1)

/* 从设备树获取 SPI Flash 芯片节点 */
// #define SPI_FLASH_NODE DT_NODELABEL(w25q128)

/* 通过设备树中的分区名，将文件系统绑定 storage 分区 */
#define STORAGE_PARTITION PARTITION_ID(storage_partition)

/* 配置 LittleFS 文件系统 */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(lfs_data);

static struct fs_mount_t lfs_storage = {
    .type = FS_LITTLEFS,
    .fs_data = &lfs_data,
    .storage_dev = (void *)STORAGE_PARTITION,
    .mnt_point = "/lfs",    /* 挂载点名称 */
};

/* 将设备树节点转换为代码里可以使用的 GPIO 规格体 */
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED, gpios);
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED, gpios);

int main(void)
{
    LOG_INF("STM32F407 Blinky Demo Started!");

    struct fs_file_t file;
    fs_file_t_init(&file);

    /* 挂载文件系统 */
    int rc = fs_mount(&lfs_storage);
    if (rc < 0) {
        LOG_ERR("Error: Failed to mount LittleFS!");
        return 0;
    }
    LOG_INF("LittleFS mounted successfully at %s", lfs_storage.mnt_point);

    /* 打开/创建文件并写入数据 */
    rc = fs_open(&file, "/lfs/config.txt", FS_O_CREATE | FS_O_RDWR);
    if (rc < 0) {
        LOG_ERR("Error: Failed to open file!");
        return 0;
    }

    /* 写入数据 */
    const static char *config_data = "device_id=STM32F407\r\nmode=active\r\n";
    ssize_t bytes_written = fs_write(&file, config_data, strlen(config_data));
    if (bytes_written < 0) {
        LOG_ERR("Error: Failed to write to file!");
        fs_close(&file);
        return 0;
    }
    LOG_INF("Wrote %zd bytes to config.txt", bytes_written);

    fs_close(&file);

    /* 读取文件验证 */
    rc = fs_open(&file, "/lfs/config.txt", FS_O_READ);
    if (rc < 0) {
        LOG_ERR("Error: Failed to open file for reading!");
        return 0;
    }

    char read_buf[64] = {0};
    ssize_t bytes_read = fs_read(&file, read_buf, sizeof(read_buf) - 1);
    if (bytes_read < 0) {
        LOG_ERR("Error: Failed to read from file!");
        fs_close(&file);
        return 0;
    }
    LOG_INF("Read %d bytes from config.txt:\n", bytes_read);
    LOG_HEXDUMP_INF(read_buf, bytes_read, "File content:");

    fs_close(&file);

    /* 获取 SPI Flash 设备 */
    // const struct device *flash_dev = DEVICE_DT_GET(SPI_FLASH_NODE);
    // if (!device_is_ready(flash_dev)) {
    //     LOG_ERR("Error: SPI Flash device is not ready!");
    //     return 0;
    // }

    // LOG_INF("SPI Flash device is ready: %s", flash_dev->name);

    /* 检查设备树里配置的 GPIO 控制器是否已经就绪 */
    if (!gpio_is_ready_dt(&blue_led) || !gpio_is_ready_dt(&red_led)) {
        LOG_ERR("Error: GPIO device is not ready!");
        return 0;
    }

    /* 配置引脚为输出模式
     * 注意：在 overlay 里写了 ACTIVE_LOW，这里 Zephyr 会自动处理极性，
     * 只需要关心“逻辑上”的初始化。
     */
    if ( gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE) < 0 ||\
         gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ) {
        LOG_ERR("Error: Failed to configure GPIO pin!");
        return 0;
    }

    // /* 准备 SPI FLASH 测试数据和地址 */
    // uint32_t test_addr = 0x000000; // 从地址 0 开始测试
    // uint8_t tx_buf[4] = {0xDE, 0xAD, 0xBE, 0xEF}; // 用于写入的数据
    // uint8_t rx_buf[4] = {0}; // 用于读取回来的数据

    // /* 擦除扇区 (W25Q128 的最小擦除单元为 4KB) */
    // LOG_INF("Erasing SPI Flash sector at address 0x%06X...", test_addr);
    // if (flash_erase(flash_dev, test_addr, 4096) < 0) {
    //     LOG_ERR("Error: Failed to erase SPI Flash!");
    //     return 0;
    // }

    // /* 写入数据 */
    // LOG_INF("Writing data to SPI Flash...");
    // if (flash_write(flash_dev, test_addr, tx_buf, sizeof(tx_buf)) < 0) {
    //     LOG_ERR("Error: Failed to write to SPI Flash!");
    //     return 0;
    // }

    // /* 读取数据 */
    // LOG_INF("Reading data back from SPI Flash...");
    // if (flash_read(flash_dev, test_addr, rx_buf, sizeof(rx_buf)) < 0) {
    //     LOG_ERR("Error: Failed to read from SPI Flash!");
    //     return 0;
    // }

    // /* 验证数据 */
    // if (memcmp(tx_buf, rx_buf, sizeof(tx_buf)) == 0) {
    //     LOG_INF("SPI Flash read/write test PASSED!");
    // } else {
    //     LOG_ERR("SPI Flash read/write test FAILED!");
    //     LOG_HEXDUMP_ERR(tx_buf, sizeof(tx_buf), "Expected:");
    //     LOG_HEXDUMP_ERR(rx_buf, sizeof(rx_buf), "Received:");
    // }

    /* 翻转电平 */
    while (1) {
        /* 翻转引脚电平 */
        gpio_pin_toggle_dt(&blue_led);
        gpio_pin_toggle_dt(&red_led);
        LOG_INF("LED toggled!");

        k_msleep(500);
    }
    return 0;
}