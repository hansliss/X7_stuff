#ifndef X7_DISPLAY_H
#define X7_DISPLAY_H
#define X7_DISPLAY_WIDTH 363
#define X7_DISPLAY_HEIGHT 272
#define X7_DISPLAY_LEFT 58
void x7_display_init(void);
void x7_display_convert(unsigned short *,const unsigned char *,const unsigned short *);
#endif
