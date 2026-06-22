#ifndef _LED_H_
#define _LED_H_

#define LED_STACK_SIZE      (512)
#define LED_PRIORITY        (7)

typedef enum {
    LED1 = 0,
    LED2,
    LED3,
    LED4,
    LED_MAX
} led_id_t;

typedef enum {
    LED_CMD_ON = 0,
    LED_CMD_OFF,
    LED_CMD_TOGGLE
} led_cmd_t;

struct led_msg {
    led_id_t led_id;
    led_cmd_t cmd;
};

/**
 * @brief Initialize all LEDs (LED1 to LED4)
 * @return 0 on success, negative error code on failure
 */
int led_init(void);

/**
 * @brief Turn on specified LED
 * @param led_id ID of the LED to turn on (LED1 to LED4)
 * @return 0 on success, negative error code on failure
 */
int led_on(led_id_t led_id);

/**
 * @brief Turn off specified LED
 * @param led_id ID of the LED to turn off (LED1 to LED4)
 * @return 0 on success, negative error code on failure
 */
int led_off(led_id_t led_id);

/**
 * @brief Toggle specified LED
 * @param led_id ID of the LED to toggle (LED1 to LED4)
 * @return 0 on success, negative error code on failure
 */
int led_toggle(led_id_t led_id);

#endif /* _LED_H_ */