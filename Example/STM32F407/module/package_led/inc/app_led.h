#ifndef APP_LED_H
#define APP_LED_H

/**
 * @brief 打开状态 LED
 */
int app_led_on(void);

/**
 * @brief 关闭状态 LED
 */
int app_led_off(void);

/**
 * @brief 翻转状态 LED 的电平
 */
int app_led_toggle(void);

#endif /* APP_LED_H */