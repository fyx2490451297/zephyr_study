#ifndef LCD_BACKLIGHT_H_
#define LCD_BACKLIGHT_H_

#include <stdbool.h>

/**
 * @brief Turn the LCD backlight on.
 *
 * Drives the `lcd-backlight` GPIO (P0.31) high, which switches on the
 * external MOSFET feeding the backlight LEDs.
 *
 * @return 0 on success, negative errno on failure.
 */
int lcd_backlight_on(void);

/**
 * @brief Turn the LCD backlight off.
 * @return 0 on success, negative errno on failure.
 */
int lcd_backlight_off(void);

/**
 * @brief Set the LCD backlight state.
 * @param on true to turn the backlight on, false to turn it off.
 * @return 0 on success, negative errno on failure.
 */
int lcd_backlight_set(bool on);

#endif /* LCD_BACKLIGHT_H_ */
