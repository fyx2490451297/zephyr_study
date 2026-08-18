#ifndef LCD_UI_PIPE_BENDER_H_
#define LCD_UI_PIPE_BENDER_H_

/**
 * @brief Build the pipe-bender dashboard UI on the active screen.
 *
 * Ported from the original LVGL v9 PC prototype to LVGL v8 widget/image
 * APIs. Shows live/target angle boxes with +/- controls, nav buttons and a
 * status bar with wifi/link icons and battery/angle gauges.
 */
void pipe_bender_ui_create(void);

#endif /* LCD_UI_PIPE_BENDER_H_ */
