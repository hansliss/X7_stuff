#include "fragment.h"
#include "hsv.h"
static struct vec camera_position,columns[DODECA_WIDTH],rows[DODECA_HEIGHT];
static unsigned short palette[12][256];
static int dot(struct vec a,struct vec b){return (a.x*b.x+a.y*b.y+a.z*b.z)/1024;}
static unsigned square_root(unsigned n){
 unsigned r=0,bit=1u<<30;
 while(bit>n)bit>>=2;
 while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
 return r;
}
int fragment_init(struct vec camera,struct vec right,struct vec up,
                  struct vec forward,int focal_length){
 unsigned i,j;
 /* Keep ray/plane reconstruction within the renderer's integer bounds. */
 if(focal_length<100||focal_length>1000)return -1;
 camera_position=camera;
 for(i=0;i<DODECA_WIDTH;i++){
  int u=(int)i-DODECA_WIDTH/2;
  columns[i].x=right.x*u/focal_length;
  columns[i].y=right.y*u/focal_length;
  columns[i].z=right.z*u/focal_length;
 }
 for(i=0;i<DODECA_HEIGHT;i++){
  int v=DODECA_HEIGHT/2-(int)i;
  rows[i].x=forward.x+up.x*v/focal_length;
  rows[i].y=forward.y+up.y*v/focal_length;
  rows[i].z=forward.z+up.z*v/focal_length;
 }
 for(i=0;i<12;i++)for(j=0;j<256;j++)
  palette[i][j]=hsv_gui_pixel(hsv_rgb(760+(i%4)*55,180,j));
 return 0;
}
void fragment_prepare(struct fragment_face *f,unsigned face,struct vec normal,
                      struct vec point,struct vec light){
 struct vec d={point.x-camera_position.x,point.y-camera_position.y,point.z-camera_position.z};
 f->normal=normal;f->light=light;f->plane=dot(normal,d);f->palette=face;
}
unsigned fragment_intensity(struct vec normal,struct vec position,struct vec light){
 struct vec delta={light.x-position.x,light.y-position.y,light.z-position.z};
 unsigned squared=(unsigned)(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
 unsigned length=square_root(squared);int cosine=0;unsigned intensity;
 if(length){cosine=dot(normal,delta)*1024/(int)length;if(cosine<0)cosine=0;if(cosine>1024)cosine=1024;}
 /* Same matte material as the flat demo: ambient plus point-light Lambert
    diffuse and inverse-quadratic attenuation, now at this surface point. */
 intensity=64+(191*(unsigned)cosine/1024)*65536/(65536+squared/1024);
 return intensity>255?255:intensity;
}
unsigned short fragment_pixel(const struct fragment_face *f,int x,int y){
 struct vec ray={columns[x].x+rows[y].x,columns[x].y+rows[y].y,columns[x].z+rows[y].z},point;
 int denominator=dot(f->normal,ray),t;
 if(!denominator)return palette[f->palette][64];
 t=f->plane*1024/denominator;
 if(t<=0||t>32768)return palette[f->palette][64];
 point.x=camera_position.x+ray.x*t/1024;
 point.y=camera_position.y+ray.y*t/1024;
 point.z=camera_position.z+ray.z*t/1024;
 /* Guard very oblique configurations; normal scene intersections are <2 units. */
 if(point.x<-8192||point.x>8192||point.y<-8192||point.y>8192||point.z<-8192||point.z>8192)
  return palette[f->palette][64];
 return palette[f->palette][fragment_intensity(f->normal,point,f->light)];
}
