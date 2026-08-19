#ifndef LCD_UI_FONTS_H_
#define LCD_UI_FONTS_H_

#include <lvgl.h>

/* FontAwesome icons: wifi(F1EB), link(F0C1), compass-drafting(F568) */
LV_FONT_DECLARE(font_awesome_12);

/* Custom image icons: connectLED(E000), signalLED(E001), 16x16 */
LV_FONT_DECLARE(font_icons_16);

/* Bold-italic "BEND" glyphs (B, E, N, D only), 14px. Real slanted/bold
 * glyphs from LiberationSans-BoldItalic.ttf, used instead of LVGL's
 * runtime rotation transform for reliability on this display. */
LV_FONT_DECLARE(font_bend_bold_italic_14);

#define ICON_CONNECT  "\xee\x80\x80"   /* U+E000 connectLED */
#define ICON_SIGNAL   "\xee\x80\x81"   /* U+E001 signalLED  */

#endif /* LCD_UI_FONTS_H_ */
