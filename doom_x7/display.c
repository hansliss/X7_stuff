#include "x7_display.h"
/* Keep the stock 320x200 renderer. Correct its intended 4:3 display aspect
 * with a centered 363x272 nearest-neighbor output. No division per frame. */
static unsigned short columns[X7_DISPLAY_WIDTH],rows[X7_DISPLAY_HEIGHT];
void x7_display_init(void){
    unsigned i;
    for(i=0;i<X7_DISPLAY_WIDTH;i++)columns[i]=i*320/X7_DISPLAY_WIDTH;
    for(i=0;i<X7_DISPLAY_HEIGHT;i++)rows[i]=i*200/X7_DISPLAY_HEIGHT;
}
void x7_display_convert(unsigned short *out,const unsigned char *in,const unsigned short *palette){
    unsigned y,x;
    for(y=0;y<X7_DISPLAY_HEIGHT;y++){
        const unsigned char *src=in+rows[y]*320;
        unsigned short *dst=out+y*480+X7_DISPLAY_LEFT;
        for(x=0;x<X7_DISPLAY_WIDTH;x++)dst[x]=palette[src[columns[x]]];
    }
}
