#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "../hsv.h"
int main(void)
{
    unsigned h,s,v,c;
    static const unsigned offsets[3] = {5,3,1};
    assert(hsv_gui_pixel(0xff0000)==0x001f);
    assert(hsv_gui_pixel(0x00ff00)==0x07e0);
    assert(hsv_gui_pixel(0x0000ff)==0xf800);
    assert(hsv_gui_pixel(0xffffff)==0xffff);
    assert(hsv_gui_pixel(0)==0);
    assert(hsv_rgb(0,255,255)==0xff0000);
    assert(hsv_rgb(256,255,255)==0xffff00);
    assert(hsv_rgb(512,255,255)==0x00ff00);
    assert(hsv_rgb(768,255,255)==0x00ffff);
    assert(hsv_rgb(1024,255,255)==0x0000ff);
    assert(hsv_rgb(1280,255,255)==0xff00ff);
    assert(hsv_rgb(1536,255,255)==0xff0000);
    for(h=0;h<1536;h++)for(s=0;s<=255;s+=17)for(v=0;v<=255;v+=51){
        unsigned rgb=hsv_rgb(h,s,v);
        for(c=0;c<3;c++){
            double k=fmod((double)h/256+offsets[c],6);
            double wave=fmax(0,fmin(1,fmin(k,4-k)));
            int expected=(int)lround(v*(1-(double)s/255*wave));
            int actual=(rgb>>(16-8*c))&255;
            assert(abs(actual-expected)<=2);
        }
    }
    return 0;
}
