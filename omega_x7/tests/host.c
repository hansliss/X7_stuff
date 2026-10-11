#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "machine.h"
#include "render.h"
static struct omega_machine m,partitioned;
static uint16_t pixels[480*272];
static void load(const char *root,const char *name,void *dst,unsigned n){
    char path[512];snprintf(path,sizeof(path),"%s/%s",root,name);FILE *f=fopen(path,"rb");assert(f);assert(fread(dst,1,n,f)==n);assert(fgetc(f)==EOF);fclose(f);
}
static void snapshot(const char *root,unsigned frame){
    char path[512];unsigned i;snprintf(path,sizeof(path),"%s/frame-%04u.ppm",root,frame);FILE *f=fopen(path,"wb");assert(f);omega_render(&m,pixels);fprintf(f,"P6\n480 272\n255\n");
    for(i=0;i<480*272;i++){unsigned p=pixels[i];fputc(((p>>11)&31)*255/31,f);fputc(((p>>5)&63)*255/63,f);fputc((p&31)*255/31,f);}fclose(f);
    snprintf(path,sizeof(path),"%s/lines-%04u.txt",root,frame);f=fopen(path,"w");assert(f);for(i=0;i<m.line_count;i++){struct omega_line *l=&m.lines[i];fprintf(f,"%d %d %d %d %u\n",l->x0,l->y0,l->x1,l->y1,l->intensity);}fclose(f);
}
int main(int argc,char **argv){
    unsigned frame;int32_t audio[2048];clock_t begin=clock();assert(argc==3||argc==4);omega_init(&m);
    load(argv[1],"omega.m7",m.mem,4096);load(argv[1],"omega.l7",m.mem+0x1000,4096);load(argv[1],"omega.k7",m.mem+0x2000,4096);load(argv[1],"omega.j7",m.mem+0x3000,4096);
    load(argv[1],"omega.e1",m.mem+0x9000,2048);load(argv[1],"omega.f1",m.mem+0x9800,2048);load(argv[1],"sound.k5",m.soundrom,2048);load(argv[1],"dvgprom.bin",m.prom,256);
    omega_init(&partitioned);memcpy(partitioned.mem,m.mem,sizeof(m.mem));memcpy(partitioned.soundrom,m.soundrom,sizeof(m.soundrom));
    for(frame=0;frame<1200;frame++){
        omega_inputs(&m,frame>220&&frame<700,frame>220&&frame<700,frame>=160&&frame<164,frame>=200&&frame<208,frame>220?(frame/4)&63:0);
        omega_run(&m,75000);omega_audio_read(&m,audio,2048);
        if(argc==3){
        omega_inputs(&partitioned,frame>220&&frame<700,frame>220&&frame<700,frame>=160&&frame<164,frame>=200&&frame<208,frame>220?(frame/4)&63:0);
        {unsigned left=75000;while(left){unsigned n=left<723?left:723;omega_run(&partitioned,n);left-=n;}omega_audio_read(&partitioned,audio,2048);}
        assert(m.cycles==partitioned.cycles&&m.main_wait==partitioned.main_wait&&m.sound_wait==partitioned.sound_wait);
        assert(m.maincpu.pc==partitioned.maincpu.pc&&m.soundcpu.pc==partitioned.soundcpu.pc);
        assert(!memcmp(m.mem,partitioned.mem,sizeof(m.mem))&&!memcmp(m.soundram,partitioned.soundram,sizeof(m.soundram)));
        assert(!memcmp(m.ay,partitioned.ay,sizeof(m.ay))&&m.line_count==partitioned.line_count);
        assert(!memcmp(m.lines,partitioned.lines,m.line_count*sizeof(m.lines[0])));
        assert(!memcmp(m.audio,partitioned.audio,sizeof(m.audio))&&m.dc==partitioned.dc);

        }
        if(frame==39||frame==159||frame==219||frame==399||frame==799||frame==1199)snapshot(argv[2],frame);
    }
    printf("cycles=%u pc=%04x sound_pc=%04x vectors=%u lines=%u errors=%u overflow=%u sound_commands=%u ay_writes=%u peak=%u watchdog=%u CPU_seconds=%.3f\n",m.cycles,m.maincpu.pc,m.soundcpu.pc,m.vector_lists,m.line_count,m.vector_errors,m.vector_overflow,m.sound_commands,m.ay_writes,m.audio_peak,m.watchdog,(double)(clock()-begin)/CLOCKS_PER_SEC);
    assert(m.cpu_errors==0);assert(m.vector_lists>100);assert(m.vector_errors==0);assert(m.vector_overflow==0);assert(m.sound_commands>0);assert(m.ay_writes>0);assert(m.audio_peak>0);assert(m.audio_overflow==0);return 0;
}
