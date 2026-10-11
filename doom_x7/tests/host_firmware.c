/* Host integration fixture, not a firmware emulator or performance model.
 * Compiled separately with the host libc and host-width ABI header copies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "../../sysprobe/ui_abi.h"
#include "../../sysprobe/fb_video.h"
#include "../include/x7_paths.h"

extern void *process_start(void *);
static const char *test_dir,*wad_path;
static unsigned ticks,submits,allocations,live_allocs;
static int scenario,exit_loop,window_alive,video_running,blend_enabled,app_active;
static int init_count,quit_count,registered,timer_count,cal_delivered,get_msg_calls;
static void *pool;
static int mapping_forbidden,mapping_disable_count,mapping_restore_count;
static int raw_key_open,raw_key_mode;
static unsigned raw_key_state,raw_queue[10][3],raw_count;

#ifdef X7_REALTIME_AUDIO
static int audio_alive,audio_playing;
static unsigned audio_queue,audio_epoch,audio_submits,audio_nonzero;
#endif
static x7_word_t current[3];
static int (*window_cb)(struct ui_message *);
static void (*dispatcher)(struct ui_app_message *);
struct timer {void (*cb)(void *);void *arg;unsigned ms;int active;} timers[8];
#define REQUIRE(test) do{if(!(test)){fprintf(stderr,"Fixture assertion at %d: %s\n",__LINE__,#test);_exit(99);}}while(0)

int x7_forbid_key_mapping(int forbid){
    REQUIRE(app_active && (forbid==0 || forbid==1));
    REQUIRE(mapping_forbidden!=forbid);
    mapping_forbidden=forbid;
    if(forbid)mapping_disable_count++;else mapping_restore_count++;
    return 1;
}

int sys_open(const char *name,int flags,x7_u32 mode){
    char path[512];int fd,host_flags=flags&3;(void)mode;
    if(!strcmp(name,"/dev/fb"))return 9999;
    if(!strcmp(name,"/dev/key")){REQUIRE(!raw_key_open);raw_key_open=1;return 9998;}
    if(!strcmp(name,X7_DOOM_WAD_PATH)){if(scenario==1)return -2;snprintf(path,sizeof(path),"%s",wad_path);}
    else {
        REQUIRE(!strncmp(name,X7_DOOM_RESOURCE_DIR "/",sizeof(X7_DOOM_RESOURCE_DIR))||!strncmp(name,"/mnt/card/",10));
        snprintf(path,sizeof(path),"%s/%s",test_dir,strrchr(name,'/')?strrchr(name,'/')+1:name);
    }
    if(flags&0x200)host_flags|=O_CREAT;
    if(flags&0x400)host_flags|=O_TRUNC;
    fd=open(path,host_flags,0600);return fd<0?-2:fd+1000;
}
int sys_close(int fd){if(fd==9998){REQUIRE(raw_key_open&&!raw_key_mode);raw_key_open=0;return 0;}if(fd==9999)return 0;return close(fd-1000);}
x7_ssize_t sys_read(int fd,void *p,x7_size_t n){if(fd==9998){unsigned bytes=raw_count*12;REQUIRE(raw_key_open&&raw_key_mode==1&&n==120);if(!bytes)return -1;memcpy(p,raw_queue,bytes);raw_count=0;return bytes;}return read(fd-1000,p,n>4096?4096:n);}
x7_ssize_t sys_write(int fd,const void *p,x7_size_t n){return write(fd-1000,p,n>97?97:n);}
x7_off_t sys_lseek(int fd,x7_off_t o,int w){return lseek(fd-1000,o,w);}
int sys_fstat(int fd,struct x7_stat *out){struct stat s;if(fstat(fd-1000,&s)<0)return -2;out->st_size=s.st_size;return 0;}
int sys_fsync(int fd){return fsync(fd-1000);}
int sys_rename(const char *old_path,const char *new_path){
    char old_name[512],new_name[512];
    REQUIRE(!strncmp(old_path,X7_DOOM_RESOURCE_DIR "/",sizeof(X7_DOOM_RESOURCE_DIR)));
    REQUIRE(!strncmp(new_path,X7_DOOM_RESOURCE_DIR "/",sizeof(X7_DOOM_RESOURCE_DIR)));
    snprintf(old_name,sizeof(old_name),"%s/%s",test_dir,strrchr(old_path,'/')+1);
    snprintf(new_name,sizeof(new_name),"%s/%s",test_dir,strrchr(new_path,'/')+1);
    if(scenario==17)return -5;
    return rename(old_name,new_name);
}
x7_u32 OSTimeGet(void){return ticks;}
void OSTimeDly(x7_u16 n){ticks+=n;}

/* Low addresses retain the engine's 32-bit pointer assumptions on x86-64.
 * Every allocation has an inaccessible page after its mapping. */
void *x7_sys_kmalloc(x7_size_t n,x7_u32 flags){
    size_t size=(n+sizeof(size_t)+4095)&~(size_t)4095;size_t *p;
    REQUIRE(flags==X7_GFP_KERNEL);
    if(scenario==7&&n>2000000)return 0;
    p=mmap(0,size+4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(p==MAP_FAILED)return 0;
    REQUIRE((size_t)p<0x80000000u);mprotect((char *)p+size,4096,PROT_NONE);
    p[0]=size+4096;live_allocs++;allocations++;return p+1;
}
void x7_sys_kfree(const void *pointer){size_t *p=(size_t *)pointer-1;size_t size=p[0];REQUIRE(live_allocs>0);live_allocs--;munmap(p,size);}
void *x7_syscall_114(x7_size_t n,x7_u32 flags){REQUIRE(flags==10);if(scenario==2)return 0;pool=x7_sys_kmalloc(n,X7_GFP_KERNEL);return pool;}
int x7_syscall_115(void *p){REQUIRE(!video_running);REQUIRE(p==pool);x7_sys_kfree(p);pool=0;return 0;}
x7_word_t x7_syscall_116(const void *p){return (size_t)p;}
int sys_ioctl(int fd,x7_u32 cmd,x7_word_t arg){
    x7_u32 *words=(void *)arg;x7_word_t *addr=(void *)arg;
    if(fd==9998){REQUIRE(raw_key_open&&cmd==0x4003&&(arg==0||arg==1));raw_key_mode=arg;raw_count=0;return 0;}
    REQUIRE(fd==9999);
    switch(cmd){
    case 0x4662:{unsigned v[11]={480,272,16,960,5,11,6,5,5,0,1};memcpy(words,v,sizeof(v));break;}
    case 0x4675: REQUIRE(window_alive);break;
    case 0x4676:blend_enabled=1;break;
    case 0x5012:REQUIRE(words[0]==480&&words[1]==272&&words[7]==1);break;
    case 0x4660:video_running=1;break;
    case 0x4666:REQUIRE(video_running&&blend_enabled);break;
    case 0x5011:if(scenario==3&&submits>=3)return -5;memcpy(addr,current,sizeof(current));break;
    case 0x4667:{
        unsigned short *p=(void *)addr[0];unsigned i;
        REQUIRE(video_running&&blend_enabled);REQUIRE(addr[0]!=current[0]);
        for(i=0;i<480*272;i++){unsigned x=i%480,y=i/480;if(
#ifdef X7_PLAYABLE
 x<58||x>=421
#else
 x<80||x>=400||y<36||y>=236
#endif
 ){
#ifdef X7_PLAYABLE
 (void)y;
#endif
#if defined(X7_REALTIME_AUDIO) && !defined(X7_PLAYABLE)
 if(x>=218&&x<260&&y>=8&&y<26)REQUIRE(p[i]==0||p[i]==0xffff);
 else
#endif
 REQUIRE(!p[i]);
 }}
        current[0]=addr[0];submits++;
        if(submits==300){char path[512];FILE *image;snprintf(path,sizeof(path),"%s/frame.ppm",test_dir);image=fopen(path,"wb");REQUIRE(image);fprintf(image,"P6\n320 200\n255\n");for(i=0;i<320*200;i++){unsigned v=p[(i/320+36)*480+i%320+80];unsigned char rgb[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,1,3,image);}fclose(image);}
        break;
    }
    case 0x4661:if(scenario==5)return -5;video_running=0;break;
    case 0x4677:blend_enabled=0;break;
    case 0x4679:break;
    default:REQUIRE(0);
    }
    return 0;
}
void *x7_dlopen(const char *name,int flags){(void)flags;if(scenario==8&&!strcmp(name,"gui.so"))return 0;return (void *)1;}
int x7_dlclose(void *p){REQUIRE(p);return 0;}
char *x7_dlerror(void){return "mock error";}
int app_getpid(void){return 7;}
struct app_process_prefix *app_get_process(int pid){static char *argv[]={"calculat.app",0};static struct app_process_prefix process;(void)pid;process.argv=argv;return &process;}
void app_ui_init(int argc,char **argv,void *p){(void)p;REQUIRE(argc==1&&argv);app_active=1;init_count++;}
void app_ui_quit(void){REQUIRE(app_active);app_active=0;quit_count++;}
void app_exit(int status){
    REQUIRE(!window_alive&&!registered&&!timer_count&&!app_active);
    REQUIRE(!raw_key_open&&!raw_key_mode);
    REQUIRE(init_count==quit_count);
    REQUIRE(!mapping_forbidden&&mapping_disable_count==mapping_restore_count);
#ifdef X7_PLAYABLE
    REQUIRE(mapping_disable_count==init_count);
#endif
#ifdef X7_REALTIME_AUDIO
    REQUIRE(!audio_alive&&!audio_playing);
    if(scenario==0){REQUIRE(audio_submits>0&&audio_nonzero>0);}
#endif
    if(scenario==5){REQUIRE(pool&&video_running&&live_allocs==1);}
    else REQUIRE(!pool&&!video_running&&!live_allocs);
    fprintf(stderr,"Scenario %d: exit %d, submitted %u frames, %u allocations; cleanup checked\n",scenario,status,submits,allocations);
    _exit(status);
}
int ui_create_window(int x,int y,int w,int h,unsigned flags,int (*cb)(struct ui_message *),unsigned parent){(void)flags;(void)parent;REQUIRE(x==0&&y==0&&w==480&&h==272);window_alive=1;window_cb=cb;return 1;}
void ui_set_focus(unsigned w){REQUIRE(w==1);}
void ui_delete_window(unsigned w){REQUIRE(w==1&&!timer_count);window_alive=0;}
void *ui_dc_get(unsigned w){REQUIRE(w==1);return (void *)1;}
void ui_background_color(void *dc,unsigned c){REQUIRE(dc&&c==0);}
void ui_clear_rect(void *dc,int x,int y,int w,int h){REQUIRE(dc&&x==0&&y==0&&w==479&&h==271);}
void ui_screen_update(void){}
int ui_default_callback(struct ui_message *msg){(void)msg;return 0;}
void ui_register_dispatcher(void (*cb)(struct ui_app_message *)){dispatcher=cb;registered=1;}
void ui_unregister_dispatcher(void (*cb)(struct ui_app_message *)){REQUIRE(cb==dispatcher);registered=0;}
void ui_exit_loop(void){exit_loop=1;}
int ui_set_timer(unsigned ms,void (*cb)(void *),void *arg){unsigned i;if(scenario==6&&ms==1)return -1;for(i=0;i<8;i++)if(!timers[i].active){timers[i]=(struct timer){cb,arg,ms,1};timer_count++;exit_loop=0;return i;}return -1;}
void ui_kill_timer(int n){REQUIRE(n>=0&&n<8&&timers[n].active);timers[n].active=0;timer_count--;}
static void deliver_key(struct ui_message *msg){
    if(raw_key_mode){
        struct ui_key *key=msg->data.key;
        REQUIRE(raw_count<10);
        if(key->type&0x07000000)raw_key_state&=~key->val;
        else raw_key_state|=key->val;
        raw_queue[raw_count][0]=0;raw_queue[raw_count][1]=raw_key_state;
        raw_queue[raw_count++][2]=0x80808080;
    }else window_cb(msg);
}
int ui_get_msg(struct ui_app_message *msg){
    unsigned calls=0;(void)msg;
    REQUIRE(++get_msg_calls==1);
    while(!exit_loop){
        unsigned i;
        for(i=0;i<8&&!exit_loop;i++)if(timers[i].active){
            if(timers[i].ms==5000||timers[i].ms==1000){if(cal_delivered)continue;cal_delivered=1;ticks+=timers[i].ms/10;}else {ticks++;
#ifdef X7_REALTIME_AUDIO
                if(scenario==13&&audio_submits>12&&calls%10==0)ticks+=14;
#endif
            }
#ifdef X7_PLAYABLE
            if(scenario==4&&calls<2){struct ui_key key={0x400,calls?0x04000000:0x20000000};struct ui_message e={.msgid=13,.data.key=&key};deliver_key(&e);}
#endif
            if(scenario==4&&submits>=4&&calls>=4){struct ui_key key={
#ifdef X7_PLAYABLE
                0x4000,
#else
                8,
#endif
                0x20000000};struct ui_message event={.msgid=13,.data.key=&key};deliver_key(&event);if(!exit_loop)timers[i].cb(timers[i].arg);}
            else {
#ifdef X7_PLAYABLE
                if((scenario==14||scenario==17)&&cal_delivered){
                    unsigned mask=0,type=0x20000000;
                    if(calls==1)mask=0x400;
                    if(calls==3){mask=0x400;type=0x04000000;}
                    /* Hold each menu press across multiple 35Hz tics. */
                    if(calls==5||calls==20||calls==35)mask=0x400;
                    if(calls==11||calls==26||calls==41){mask=0x400;type=0x04000000;}
                    if(calls==60)mask=0x10;
                    if(calls==80)mask=0x100;
                    if(calls==120){mask=0x10;type=0x04000000;}
                    if(calls==140){mask=0x100;type=0x04000000;}
                    if(calls==160)mask=0x400;
                    if(calls==220){mask=0x400;type=0x01000000;}
                    if(calls==240||calls==270)mask=8;
                    if(calls==250||calls==280){mask=8;type=0x04000000;}
                    if(calls==320)mask=0x2000;
                    if(calls==330){mask=0x2000;type=0x04000000;}
                    if(calls==360)mask=0x1000;
                    if(calls==370){mask=0x1000;type=0x04000000;}
                    if(calls==600)mask=8;
                    if(calls==610){mask=8;type=0x04000000;}
                    if(calls==622||calls==632||calls==642||calls==652||calls==662)mask=0x20;
                    if(calls==628||calls==638||calls==648||calls==658||calls==668){mask=0x20;type=0x04000000;}
                    if(calls==675||calls==705)mask=0x400;
                    if(calls==685){mask=0x400;type=0x04000000;}
                    if(mask){struct ui_key key={mask,type};struct ui_message e={.msgid=13,.data.key=&key};deliver_key(&e);}
                }
                if(scenario==5&&(calls==1||calls==3)){struct ui_key key={0x400,calls==1?0x20000000:0x04000000};struct ui_message e={.msgid=13,.data.key=&key};deliver_key(&e);}
                if(scenario==5&&calls==500){struct ui_key key={0x4000,0x20000000};struct ui_message e={.msgid=13,.data.key=&key};deliver_key(&e);}
#endif
                if(!exit_loop)timers[i].cb(timers[i].arg);
            }
        }
        REQUIRE(++calls<20000);
    }
    return 0;
}
void ui_dispatch_msg(struct ui_app_message *msg){(void)msg;}
int main(int argc,char **argv){REQUIRE(argc==4);test_dir=argv[1];wad_path=argv[2];scenario=atoi(argv[3]);process_start(0);REQUIRE(0);}

#ifdef X7_REALTIME_AUDIO
struct host_pcm {void *planes[6];int channels,frames,precision;};
void *audio_open(void){if(scenario==9)return 0;audio_alive=1;audio_epoch=ticks;return (void *)2;}
int audio_command(void *handle,unsigned cmd,x7_word_t arg){
 REQUIRE(handle==(void *)2&&audio_alive);
 switch(cmd){
 case 0x4d414f01:{int *f=(void *)arg;REQUIRE(f[0]==2&&f[1]==44100&&f[2]==0);break;}
 case 0x4d414f32:REQUIRE(arg==2048);break;
 case 0x4d414f33:REQUIRE(arg==8);break;
 case 0x4d414f02:if(scenario==12)return -1;audio_playing=1;break;
 case 0x4d414f03:audio_playing=0;break;
 case 0x4d414f45:{unsigned consumed=(ticks-audio_epoch)*441;audio_epoch=ticks;if(scenario==11)return -1;/* Model a retained/partial sector: feeding must continue above 1024. */
 if(scenario==13&&audio_submits)REQUIRE(consumed<=audio_queue);
 audio_queue=consumed>=audio_queue?0:audio_queue-consumed;
 if(scenario!=13&&audio_submits>=3&&audio_queue<1536)audio_queue=1536;*(int *)arg=audio_queue;break;}
 default:REQUIRE(0);
 }
 return 0;
}
int audio_submit(void *handle,struct host_pcm *p){
 int i,*data=p->planes[0];REQUIRE(handle==(void *)2&&audio_alive&&audio_playing);
 REQUIRE(p->channels==2&&p->frames==1024&&p->precision==0&&p->planes[1]==p->planes[0]);
 if(scenario==10)return -1;
 for(i=0;i<1024;i++){
   /* Model the final precision=0 saturating conversion, not just the
    * optional intermediate <<8 (which also changes precision to 8). */
   REQUIRE(data[i]>=-32768&&data[i]<=32767);
   if(data[i])audio_nonzero++;
   data[i]=(int)((unsigned)data[i]<<16);
 }
 audio_queue+=1024;audio_submits++;return 0;
}
void audio_close(void *h){REQUIRE(h==(void *)2&&audio_alive&&!audio_playing);audio_alive=0;}
#endif
