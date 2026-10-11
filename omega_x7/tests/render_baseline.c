/* Preserved v4 renderer, linked only by the comparison benchmark. */
#include "../render.h"
static int code(int x,int y){return (x<0?1:0)|(x>1023?2:0)|(y<0?4:0)|(y>1023?8:0);}
static int clip(int *x0,int *y0,int *x1,int *y1){
    unsigned n;
    for(n=0;n<8;n++){
        int a=code(*x0,*y0),b=code(*x1,*y1),c,x,y;
        if(!(a|b))return 1;
        if(a&b)return 0;
        c=a?a:b;
        if(c&8){y=1023;x=*x0+(*x1-*x0)*(y-*y0)/(*y1-*y0);}
        else if(c&4){y=0;x=*x0+(*x1-*x0)*(y-*y0)/(*y1-*y0);}
        else if(c&2){x=1023;y=*y0+(*y1-*y0)*(x-*x0)/(*x1-*x0);}
        else{x=0;y=*y0+(*y1-*y0)*(x-*x0)/(*x1-*x0);}
        if(c==a){*x0=x;*y0=y;}else{*x1=x;*y1=y;}
    }
    return 0;
}
void omega_render_baseline(const struct omega_machine *m,uint16_t *pixels){
    unsigned i;
    for(i=0;i<OMEGA_WIDTH*OMEGA_HEIGHT;i++)pixels[i]=0;
    for(i=0;i<m->line_count;i++){
        const struct omega_line *l=&m->lines[i];int x0=l->x0,y0=l->y0,x1=l->x1,y1=l->y1,dx,dy,sx,sy,err;
        unsigned brightness=l->intensity*17,r=brightness>>3,g=brightness>>2;uint16_t color=(uint16_t)((r<<11)|(g<<5)|r);
        if(!clip(&x0,&y0,&x1,&y1))continue;
        x0=104+(x0*271+511)/1023;x1=104+(x1*271+511)/1023;y0=(y0*271+511)/1023;y1=(y1*271+511)/1023;
        dx=x1>x0?x1-x0:x0-x1;dy=-(y1>y0?y1-y0:y0-y1);sx=x0<x1?1:-1;sy=y0<y1?1:-1;err=dx+dy;
        for(;;){
            unsigned at=(unsigned)y0*OMEGA_WIDTH+(unsigned)x0;int e;
            if(color>pixels[at])pixels[at]=color;
            if(x0==x1&&y0==y1)break;
            e=err*2;
            if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}
        }
    }
}
