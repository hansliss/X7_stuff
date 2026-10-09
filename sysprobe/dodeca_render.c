#include "dodeca_render.h"
#include "hsv.h"
#include "dodeca_mesh.h"
const struct dodeca_scene dodeca_default_scene = {
    {3*1024,2*1024,6*1024}, {0,0,0}, {0,1024,0}, {-2*1024,3*1024,2*1024}, 300
};
static struct dodeca_scene scene;
static struct vec forward,right,up;
struct point { int x,y; };
static struct vec sub(struct vec a,struct vec b) {struct vec c={a.x-b.x,a.y-b.y,a.z-b.z};return c;}
static int dot(struct vec a,struct vec b) {return (a.x*b.x+a.y*b.y+a.z*b.z)/1024;}
static struct vec cross(struct vec a,struct vec b) {
 struct vec c={(a.y*b.z-a.z*b.y)/1024,(a.z*b.x-a.x*b.z)/1024,(a.x*b.y-a.y*b.x)/1024};return c;
}
static unsigned root(unsigned n) {
 unsigned r=0,bit=1u<<30;
 while(bit>n)bit>>=2;
 while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}return r;
}
static struct vec normalize(struct vec a) {
 unsigned len=root((unsigned)(a.x*a.x+a.y*a.y+a.z*a.z));
 if(len){a.x=a.x*1024/(int)len;a.y=a.y*1024/(int)len;a.z=a.z*1024/(int)len;}return a;
}
static int bounded(struct vec a,int limit) {
 return a.x>=-limit&&a.x<=limit&&a.y>=-limit&&a.y<=limit&&a.z>=-limit&&a.z<=limit;
}
int dodeca_init(const struct dodeca_scene *config) {
 if(!config||config->focal_length<1||config->focal_length>1000||
    !bounded(config->camera_position,8192)||!bounded(config->look_at,8192)||
    !bounded(config->light_position,8192)||!bounded(config->up,1024))return -1;
 scene=*config;forward=normalize(sub(scene.look_at,scene.camera_position));
 right=normalize(cross(forward,scene.up));up=cross(right,forward);
 if(dot(forward,forward)<1000||dot(right,right)<1000)return -1;
 return 0;
}
static struct vec rotate(struct vec a,unsigned frame) {
 int s=sine[(frame*2)&255],c=sine[(frame*2+64)&255],x,y,z;
 x=(a.x*c+a.z*s)/1024;z=(-a.x*s+a.z*c)/1024;a.x=x;a.z=z;
 s=sine[(frame+25)&255];c=sine[(frame+89)&255];
 y=(a.y*c-a.z*s)/1024;z=(a.y*s+a.z*c)/1024;a.y=y;a.z=z;
 s=sine[(frame+9)&255];c=sine[(frame+73)&255];
 x=(a.x*c-a.y*s)/1024;y=(a.x*s+a.y*c)/1024;a.x=x;a.y=y;return a;
}
static int project(struct vec a,struct point *p) {
 struct vec d=sub(a,scene.camera_position);int z=dot(d,forward);
 if(z<256)return 0;
 p->x=DODECA_WIDTH/2+dot(d,right)*scene.focal_length/z;
 p->y=DODECA_HEIGHT/2-dot(d,up)*scene.focal_length/z;
 /* Reject unbounded projected coordinates before edge arithmetic. */
 if(p->x < -4096||p->x > 4096||p->y < -4096||p->y > 4096)return 0;
 return 1;
}
static int edge(struct point a,struct point b,int x,int y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static int minimum(int a,int b){return a<b?a:b;}
static int maximum(int a,int b){return a>b?a:b;}
#ifdef PROBE_FAST
/* Walk the three edges once; fill contiguous spans with native 32-bit stores. */
static void triangle(unsigned short *pixels,struct point a,struct point b,struct point c,unsigned short color) {
 struct point v[3]={a,b,c};int lo[3],hi[3],pos[3],step[3],i,y;
 int miny=maximum(0,minimum(a.y,minimum(b.y,c.y)));
 int maxy=minimum(DODECA_HEIGHT-1,maximum(a.y,maximum(b.y,c.y)));
 unsigned packed=(unsigned)color|((unsigned)color<<16);
 if(!edge(a,b,c.x,c.y))return;
 for(i=0;i<3;i++){
  struct point p=v[i],q=v[(i+1)%3];
  if(p.y>q.y){struct point t=p;p=q;q=t;}
  lo[i]=maximum(miny,p.y);hi[i]=minimum(maxy,q.y);
  if(p.y==q.y){lo[i]=1;hi[i]=0;step[i]=pos[i]=0;continue;}
  step[i]=(q.x-p.x)*65536/(q.y-p.y);
  pos[i]=p.x*65536+step[i]*(lo[i]-p.y);
 }
 for(y=miny;y<=maxy;y++){
  int left=0x7fffffff,right=-0x7fffffff,x,last;
  for(i=0;i<3;i++)if(y>=lo[i]&&y<=hi[i]){
   left=minimum(left,pos[i]);right=maximum(right,pos[i]);pos[i]+=step[i];
  }
  if(left>right)continue;
  x=maximum(0,(left+65535)>>16);last=minimum(DODECA_WIDTH-1,right>>16);
  if(x>last)continue;
  if(x&1)pixels[y*DODECA_WIDTH+x++]=color;
  while(x+1<=last){
   /* Buffers are aligned and the row width is even. Use may_alias to keep
      16-bit outline writes and 32-bit span writes well-defined for GCC. */
   typedef unsigned alias_u32 __attribute__((may_alias));
   *(alias_u32 *)(pixels+y*DODECA_WIDTH+x)=packed;x+=2;
  }
  if(x<=last)pixels[y*DODECA_WIDTH+x]=color;
 }
}
#else
static void triangle(unsigned short *pixels,struct point a,struct point b,struct point c,unsigned short color) {
 int minx,maxx,miny,maxy,x,y,w0,w1,w2,row0,row1,row2;
 if(edge(a,b,c.x,c.y)<0){struct point t=b;b=c;c=t;}
 if(!edge(a,b,c.x,c.y))return;
 minx=maximum(0,minimum(a.x,minimum(b.x,c.x)));maxx=minimum(DODECA_WIDTH-1,maximum(a.x,maximum(b.x,c.x)));
 miny=maximum(0,minimum(a.y,minimum(b.y,c.y)));maxy=minimum(DODECA_HEIGHT-1,maximum(a.y,maximum(b.y,c.y)));
 row0=edge(b,c,minx,miny);row1=edge(c,a,minx,miny);row2=edge(a,b,minx,miny);
 for(y=miny;y<=maxy;y++){
  w0=row0;w1=row1;w2=row2;
  for(x=minx;x<=maxx;x++){
   if(w0>=0&&w1>=0&&w2>=0)pixels[y*DODECA_WIDTH+x]=color;
   w0-=c.y-b.y;w1-=a.y-c.y;w2-=b.y-a.y;
  }
  row0+=c.x-b.x;row1+=a.x-c.x;row2+=b.x-a.x;
 }
}
#endif
static void pixel(unsigned short *p,int x,int y,unsigned short color){if(x>=0&&y>=0&&x<DODECA_WIDTH&&y<DODECA_HEIGHT)p[y*DODECA_WIDTH+x]=color;}
static void line(unsigned short *p,struct point a,struct point b,unsigned short color){
 int dx=a.x<b.x?b.x-a.x:a.x-b.x,sx=a.x<b.x?1:-1,dy=a.y<b.y?a.y-b.y:b.y-a.y,sy=a.y<b.y?1:-1,err=dx+dy;
 unsigned limit=0;
 while(limit++<2048){int twice;pixel(p,a.x,a.y,color);if(a.x==b.x&&a.y==b.y)break;twice=2*err;if(twice>=dy){err+=dy;a.x+=sx;}if(twice<=dx){err+=dx;a.y+=sy;}}
}
unsigned dodeca_render(unsigned short *pixels,unsigned frame){
 struct vec world[20];struct point screen[20];
 unsigned order[12],count=0,i,j,y,x;int depths[12];unsigned short colors[12];
 #ifdef PROBE_FAST
 {
  typedef unsigned alias_u32 __attribute__((may_alias));
  unsigned short bg=hsv_gui_pixel(0x101828);unsigned packed=(unsigned)bg|((unsigned)bg<<16);
  alias_u32 *p=(alias_u32 *)pixels;
  for(x=0;x<DODECA_WIDTH*DODECA_HEIGHT/2;x++)p[x]=packed;
 }
 (void)y;
#else
 for(y=0;y<DODECA_HEIGHT;y++){
  unsigned short bg=hsv_gui_pixel(0x101828);
  for(x=0;x<DODECA_WIDTH;x++)pixels[y*DODECA_WIDTH+x]=bg;
 }
 #endif
 for(i=0;i<20;i++){world[i]=rotate(vertices[i],frame);if(!project(world[i],&screen[i]))return 0;}
 for(i=0;i<12;i++){
  struct vec center={0,0,0},normal,light;int diffuse,intensity,distance;
  for(j=0;j<5;j++){center.x+=world[faces[i][j]].x;center.y+=world[faces[i][j]].y;center.z+=world[faces[i][j]].z;}
  center.x/=5;center.y/=5;center.z/=5;
  normal=normalize(cross(sub(world[faces[i][1]],world[faces[i][0]]),sub(world[faces[i][2]],world[faces[i][0]])));
  if(dot(normal,sub(scene.camera_position,center))<=0)continue;
  light=sub(scene.light_position,center);distance=dot(light,light);light=normalize(light);
  diffuse=maximum(0,dot(normal,light));
  /* Ambient 25%; Lambert diffuse with inverse-quadratic distance attenuation. */
  intensity=64+(191*diffuse/1024)*65536/(65536+distance);
  if(intensity>255)intensity=255;
  /* Distinct blue/teal facet colors make the flat pentagons easy to read. */
  colors[i]=hsv_gui_pixel(hsv_rgb(760+(i%4)*55,180,(unsigned)intensity));
  depths[i]=dot(sub(center,scene.camera_position),forward);order[count++]=i;
 }
 /* A convex object permits painter ordering after back-face removal. */
 for(i=1;i<count;i++){unsigned f=order[i],k=i;while(k&&depths[order[k-1]]<depths[f]){order[k]=order[k-1];k--;}order[k]=f;}
 for(i=0;i<count;i++){unsigned f=order[i];for(j=1;j<4;j++)triangle(pixels,screen[faces[f][0]],screen[faces[f][j]],screen[faces[f][j+1]],colors[f]);}
 for(i=0;i<count;i++){unsigned f=order[i];for(j=0;j<5;j++)line(pixels,screen[faces[f][j]],screen[faces[f][(j+1)%5]],hsv_gui_pixel(0x182030));}
 {
  struct point bulb;if(project(scene.light_position,&bulb)){
   int dx,dy;for(dy=-4;dy<=4;dy++)for(dx=-4;dx<=4;dx++)if(dx*dx+dy*dy<=16)pixel(pixels,bulb.x+dx,bulb.y+dy,hsv_gui_pixel(0xfff4c0));
  }
 }
 return count;
}
