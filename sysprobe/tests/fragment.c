#include <assert.h>
#include <math.h>
#include "../fragment.h"
#include "../hsv.h"
static double reference(struct vec n,struct vec p,struct vec l){
 double x=l.x-p.x,y=l.y-p.y,z=l.z-p.z,d=x*x+y*y+z*z;
 double cosine=d?(n.x*x+n.y*y+n.z*z)/(1024*sqrt(d)):0;
 if(cosine<0)cosine=0;
 if(cosine>1)cosine=1;
 return 64+191*cosine*65536/(65536+d/1024);
}
int main(void){
 struct vec n={0,0,1024},camera={0,0,6144},r={1024,0,0},up={0,1024,0},fwd={0,0,-1024};
 struct vec origin={0,0,0},light={1536,2048,3072};struct fragment_face face;
 unsigned i;unsigned short first;int changed=0;
 for(i=0;i<2000;i++){
  struct vec p={(int)(i*137%8192)-4096,(int)(i*313%4096)-2048,0};
  unsigned value=fragment_intensity(n,p,light);
  assert(value>=64&&value<=255);assert(fabs((double)value-reference(n,p,light))<3);
 }
 {struct vec behind={0,0,-1024};assert(fragment_intensity(n,origin,behind)==64);}
 assert(fragment_init(camera,r,up,fwd,99)<0);
 assert(fragment_init(camera,r,up,fwd,525)==0);
 fragment_prepare(&face,0,n,origin,light);
 assert(fragment_pixel(&face,240,136)==hsv_gui_pixel(hsv_rgb(760,180,fragment_intensity(n,origin,light))));
 first=fragment_pixel(&face,120,136);
 for(i=121;i<360;i++)if(fragment_pixel(&face,(int)i,136)!=first)changed++;
 assert(changed>100); /* One planar face must have a lighting gradient. */
 return 0;
}
