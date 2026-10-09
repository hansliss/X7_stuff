#include "fb_video.h"
#define STRIDE (480u*272u*2u+1024u)
static int command(struct fb_video *v,unsigned cmd,void *arg){return sys_ioctl(v->fd,cmd,(x7_word_t)arg);}
unsigned short *fb_video_pixels(struct fb_video *v,unsigned frame){return (unsigned short *)((unsigned char *)v->pool+(frame%5)*STRIDE);}
int fb_video_open(struct fb_video *v,int (*record)(const char *,x7_u64),int (*before)(const char *)){
 x7_u32 lcd[11]={0},mode[11]={480,272,480,0,0,0,0,1,0,0,0};unsigned i;int n;
 x7_u32 blend[9]={0,0,0,0,0,0,480,272,0};
 x7_s32 ids[2]={0,-1};
 v->fd=-1;v->pool=0;v->started=0;v->blending=0;v->recent[0]=v->recent[1]=0;
 v->avoided=v->unavailable=v->query_failures=0;
 if(!before("BEFORE open /dev/fb"))return -1;
 v->fd=sys_open("/dev/fb",2,0);record("fb fd",(x7_u64)(x7_s64)v->fd);if(v->fd<0)return -1;
 if(!before("BEFORE fb LCD parameters 0x4662"))return -1;
 n=command(v,0x4662,lcd);record("fb LCD query",(x7_u64)(x7_s64)n);
 for(i=0;i<11;i++)record("fb LCD word",lcd[i]);
 if(n<0||lcd[0]!=480||lcd[1]!=272)return -1;
 if(!before("BEFORE five-buffer pool allocation"))return -1;
 v->pool=x7_syscall_114(STRIDE*5,0x0a);record("fb pool",(x7_word_t)v->pool);if(!v->pool)return -1;
 /* Initialize every buffer before the display hardware can read it. */
 for(i=0;i<5;i++){
  unsigned j;unsigned short *p=fb_video_pixels(v,i);
  for(j=0;j<480u*272u;j++)p[j]=0;
  v->address[i]=x7_syscall_116(p);record("fb buffer driver address",v->address[i]);if(!v->address[i])return -1;
 }
 if(!before("BEFORE fb create fullscreen blending 0x4675"))return -1;
 v->blending=1;n=command(v,0x4675,blend);record("fb create blending",(x7_u64)(x7_s64)n);if(n<0)return -1;
 if(!before("BEFORE fb enable blending 0x4676"))return -1;
 n=command(v,0x4676,ids);record("fb enable blending",(x7_u64)(x7_s64)n);if(n<0)return -1;
 if(!before("BEFORE fb video mode 0x5012"))return -1;
 n=command(v,0x5012,mode);record("fb video mode",(x7_u64)(x7_s64)n);if(n<0)return -1;
 if(!before("BEFORE fb start video 0x4660"))return -1;
 /* Attempt stop even if start reports an error after changing hardware state. */
 v->started=1;n=command(v,0x4660,0);record("fb start video",(x7_u64)(x7_s64)n);if(n<0)return -1;
 if(!before("BEFORE fb activate display 0x4666"))return -1;
 n=command(v,0x4666,0);record("fb activate display",(x7_u64)(x7_s64)n);return n<0?-1:0;
}
/* 0x5011 reads the driver's current three-plane address block. It is not
   a recovered hardware completion fence. Protect recent submissions too,
   covering a pending switch between this snapshot and the next submit. */
int fb_video_acquire(struct fb_video *v,unsigned preferred,unsigned *slot){
 x7_word_t active[3]={0};unsigned i,j;
 if(command(v,0x5011,active)<0){v->query_failures++;return -1;}
 for(i=0;i<5;i++){
  unsigned index=(preferred+i)%5;int protected=0;
  for(j=0;j<3;j++)if(active[j]&&v->address[index]==active[j])protected=1;
  for(j=0;j<2;j++)if(v->recent[j]&&v->address[index]==v->recent[j])protected=1;
  if(!protected){*slot=index;return 0;}
  v->avoided++;
 }
 v->unavailable++;return 1;
}
int fb_video_submit(struct fb_video *v,unsigned frame){
 x7_word_t planes[3]={v->address[frame%5],0,0};
 int n=command(v,0x4667,planes);
 if(n>=0){v->recent[1]=v->recent[0];v->recent[0]=planes[0];}
 return n;
}
int fb_video_close(struct fb_video *v,int (*record)(const char *,x7_u64),int (*before)(const char *)){
 int status=0,n;x7_s32 ids[2]={0,-1};
 if(v->started){
  before("BEFORE fb stop video 0x4661");n=command(v,0x4661,0);record("fb stop video",(x7_u64)(x7_s64)n);
  if(n<0)status=-1;else {v->started=0;OSTimeDly(3);}
 }
 if(v->blending){
  before("BEFORE fb disable blending 0x4677");n=command(v,0x4677,ids);record("fb disable blending",(x7_u64)(x7_s64)n);if(n<0)status=-1;
  before("BEFORE fb destroy blending 0x4679");n=command(v,0x4679,0);record("fb destroy blending",(x7_u64)(x7_s64)n);if(n<0)status=-1;
  v->blending=0;
 }
 /* Never free a buffer that the display driver may still be using. */
 if(v->pool&&!v->started){before("BEFORE free fb pool");record("fb pool free",(x7_u64)(x7_s64)x7_syscall_115(v->pool));v->pool=0;}
 if(v->started)record("fb pool retained after stop failure",1);
 if(v->fd>=0){record("fb close",(x7_u64)(x7_s64)sys_close(v->fd));v->fd=-1;}
 return status;
}
