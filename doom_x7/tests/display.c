#include <assert.h>
#include <stdlib.h>
#include "x7_display.h"
int main(void){
    unsigned i,x,y;unsigned char in[320*200];unsigned short palette[256];
    unsigned short *guarded=malloc((480*272+2)*sizeof(*guarded)),*out=guarded+1;
    assert(guarded);
    for(i=0;i<320*200;i++)in[i]=(i*17+i/320)%256;
    for(i=0;i<256;i++)palette[i]=i*257;
    for(i=0;i<480*272+2;i++)guarded[i]=0xa55a;
    x7_display_init();x7_display_convert(out,in,palette);
    assert(guarded[0]==0xa55a&&guarded[480*272+1]==0xa55a);
    for(y=0;y<272;y++)for(x=0;x<480;x++){
        unsigned short expected=0xa55a;
        if(x>=58&&x<421)expected=palette[in[(y*200/272)*320+(x-58)*320/363]];
        assert(out[y*480+x]==expected);
    }
    /* Last output pixel must reach the final source row and column. */
    assert(out[271*480+420]==palette[in[320*200-1]]);
    free(guarded);return 0;
}
