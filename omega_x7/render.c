#include "render.h"
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
/* Quarter coverage of the original intensity, including zero coverage. */
static void plot(uint16_t *pixels,int x,int y,uint16_t color){
    if((unsigned)x<272&&(unsigned)y<272){
        unsigned at=(unsigned)y*OMEGA_WIDTH+(unsigned)x+104;
        if(color>pixels[at])pixels[at]=color;
    }
}
static void pair(uint16_t *pixels,int x,int y,int steep,const uint16_t *shades){
    int row=y>=0?y/256:-((-y+255)/256);
    unsigned fraction=(unsigned)(y-row*256),weight=(fraction+32)/64;
    if(steep){plot(pixels,row,x,shades[4-weight]);plot(pixels,row+1,x,shades[weight]);}
    else{plot(pixels,x,row,shades[4-weight]);plot(pixels,x,row+1,shades[weight]);}
}
void omega_render(const struct omega_machine *m,uint16_t *pixels){
    uint16_t shades[16][5];unsigned i,w;
    for(i=0;i<16;i++)for(w=0;w<=4;w++){
        unsigned brightness=(i*17*w+2)/4,r=brightness>>3,g=brightness>>2;
        shades[i][w]=(uint16_t)((r<<11)|(g<<5)|r);
    }
    for(i=0;i<OMEGA_WIDTH*OMEGA_HEIGHT;i++)pixels[i]=0;
    for(i=0;i<m->line_count;i++){
        const struct omega_line *l=&m->lines[i];int x0=l->x0,y0=l->y0,x1=l->x1,y1=l->y1,dx,dy,steep,x,last,y,step,rem,carry,t;
        const uint16_t *colors=shades[l->intensity<16?l->intensity:15];
        if(!clip(&x0,&y0,&x1,&y1))continue;
        /* Keep eight fractional bits instead of rounding endpoints to pixels.
         * Select the major axis, then advance the minor coordinate exactly
         * using a quotient/remainder DDA. No floating point, 64-bit division,
         * intermediate image or per-pixel division by a variable is needed. */
        x0=(x0*271*256+511)/1023;x1=(x1*271*256+511)/1023;
        y0=(y0*271*256+511)/1023;y1=(y1*271*256+511)/1023;
        dx=x1>x0?x1-x0:x0-x1;dy=y1>y0?y1-y0:y0-y1;steep=dy>dx;
        if(steep){t=x0;x0=y0;y0=t;t=x1;x1=y1;y1=t;}
        if(x0>x1){t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
        dx=x1-x0;dy=y1-y0;x=(x0+128)/256;last=(x1+128)/256;
        /* Very short vectors still leave a visible mark at their midpoint. */
        if(x==last){pair(pixels,x,(y0+y1)/2,steep,colors);continue;}
        t=dy*(x*256-x0);y=y0+t/dx;rem=t%dx;
        step=(dy*256)/dx;carry=(dy*256)%dx;
        for(;x<=last;x++){
            pair(pixels,x,y,steep,colors);
            y+=step;rem+=carry;
            if(rem>=dx){y++;rem-=dx;}else if(rem<=-dx){y--;rem+=dx;}
        }
    }
}
