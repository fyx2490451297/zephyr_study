#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdbool.h>

/**
 * @brief Check whether the panel bound to the devicetree "zephyr,display"
 * chosen node finished initialization successfully.
 *
 * Upper layers (LVGL port, UI) must poll this instead of touching any
 * panel-specific device handle, so they stay agnostic of which concrete
 * display IC (ILI9341, ST7789, ...) is wired up.
 *
 * @return true once the display device is ready to use.
 */
bool display_is_ready(void);

/**
 * @brief Turn the panel output on (disable blanking) so previously drawn
 * frames become visible.
 *
 * @return 0 on success, negative errno on failure.
 */
int display_power_on(void);

/**
 * @brief Turn the panel output off (enable blanking).
 *
 * @return 0 on success, negative errno on failure.
 */
int display_power_off(void);

#endif /* DISPLAY_H_ */
