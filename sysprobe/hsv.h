#ifndef X7_HSV_H
#define X7_HSV_H
/* Hue: 0..1535, saturation/value: 0..255. Returns 0xRRGGBB. */
unsigned hsv_rgb(unsigned hue, unsigned saturation, unsigned value);
/* Mirrors GUI.SO 0x40c001d0: low color byte maps to high 5 bits. */
unsigned short hsv_gui_pixel(unsigned color);
#define HSV_WIDTH 480
#define HSV_HEIGHT 272
#define HSV_TILE 4
#endif
