#include "key.h"
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(key_pkg, LOG_LEVEL_INF);

static key_callback_t key_callbacks[KEY_MAX];

static int key_init(void)
{
    /* TODO: Configure GPIO interrupt-based key detection */
    LOG_INF("KEY package initialized.");
    return 0;
}

int key_register_callback(key_id_t key_id, key_callback_t cb)
{
    if (key_id >= KEY_MAX || cb == NULL) {
        return -EINVAL;
    }
    key_callbacks[key_id] = cb;
    return 0;
}

SYS_INIT(key_init, APPLICATION, CONFIG_PACKAGE_KEY_INIT_PRIORITY);
