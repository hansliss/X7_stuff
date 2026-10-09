#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../dodeca_render.h"
#include "../dodeca_mesh.h"
static _Alignas(4) unsigned short buffer[DODECA_WIDTH*DODECA_HEIGHT];
static void preview(const char *path){
 FILE *f=fopen(path,"wb");unsigned i;assert(f);fprintf(f,"P6\n%d %d\n255\n",DODECA_WIDTH,DODECA_HEIGHT);
 for(i=0;i<DODECA_WIDTH*DODECA_HEIGHT;i++){
  unsigned p=buffer[i];unsigned char rgb[3]={(unsigned char)((p&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)(((p>>11)&31)*255/31)};
  assert(fwrite(rgb,1,3,f)==3);
 }
 assert(fclose(f)==0);
}
int main(int argc,char **argv){
 unsigned edge_counts[20][20]={{0}},i,j,edges=0,frame;unsigned checksum=0,first=0;
 struct dodeca_scene bad=dodeca_default_scene;
 for(i=0;i<12;i++)for(j=0;j<5;j++){
  unsigned a=faces[i][j],b=faces[i][(j+1)%5],lo=a<b?a:b,hi=a<b?b:a;
  int dx=vertices[a].x-vertices[b].x,dy=vertices[a].y-vertices[b].y,dz=vertices[a].z-vertices[b].z;
  assert(a<20&&b<20&&a!=b);edge_counts[lo][hi]++;
  assert(abs(dx*dx+dy*dy+dz*dz-1602064)<6000);
 }
 for(i=0;i<20;i++)for(j=i+1;j<20;j++)if(edge_counts[i][j]){assert(edge_counts[i][j]==2);edges++;}
 assert(edges==30&&20-30+12==2);
 bad.look_at=bad.camera_position;assert(dodeca_init(&bad)<0);
 #ifdef PROBE_FAST
 bad=dodeca_default_scene;bad.focal_length=525;assert(dodeca_init(&bad)==0);
#else
 assert(dodeca_init(&dodeca_default_scene)==0);
#endif
 for(frame=0;frame<1200;frame++){
  unsigned count=dodeca_render(buffer,frame);assert(count>=3&&count<=6);
  /* Every frame has a clear border, proving the update box contains it. */
  for(i=0;i<DODECA_WIDTH;i++)assert(buffer[i]==buffer[0]&&buffer[(DODECA_HEIGHT-1)*DODECA_WIDTH+i]==buffer[0]);
  for(i=0;i<DODECA_HEIGHT;i++)assert(buffer[i*DODECA_WIDTH]==buffer[0]&&buffer[i*DODECA_WIDTH+DODECA_WIDTH-1]==buffer[0]);
  checksum=0;for(i=0;i<DODECA_WIDTH*DODECA_HEIGHT;i++)checksum=checksum*33+buffer[i];
  if(frame==0){first=checksum;if(argc>1)preview(argv[1]);}
  if(frame==40&&argc>2)preview(argv[2]);
 }
 assert(first!=checksum);return 0;
}
