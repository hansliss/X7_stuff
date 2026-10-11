#include <stddef.h>
#include <string.h>
#include "../sysprobe/ui_abi.h"
#include "../sysprobe/fb_video.h"
#include "machine.h"
#include "render.h"
#include "controls.h"
#include "nvram.h"
#ifndef X7_OMEGA_DEBUG_LOG
#define X7_OMEGA_DEBUG_LOG 0
#endif
/* Existing firmware interfaces verified by the DOOM port. */
int x7_forbid_key_mapping(int);
void *audio_open(void);
int audio_command(void *,x7_u32,x7_word_t);
int audio_submit(void *,void *);
void audio_close(void *);
struct pcm { void *planes[6]; int channels,frames,precision; };
static struct omega_machine machine;
static struct fb_video video={.fd=-1};
static void *gui,*applib,*soundlib,*sound;
static int log_fd=-1,key_fd=-1,window,timer=-1,cal_timer=-1,registered,initialized,mapping,raw_mode,playing;
static unsigned callbacks,callback_max,audio_ticks,audio_polls,queue_max,queue_histogram[10];
static unsigned done,busy,epoch,cal_epoch,tick_rate=100,frames,sim_ticks,draw_ticks,audio_blocks,empty_audio,angle,angle_epoch,draw_epoch,angle_phase,draw_phase;
static unsigned raw_state,filtered,toggle_since[2],rejected,quit_since;
static int status;
static unsigned char saved_nvram[OMEGA_NVRAM_SIZE];
static int nvram_ready;
static int32_t pcm_samples[1024] __attribute__((aligned(32)));
static int record(const char *tag,x7_u64 value){
    char text[160];const char hex[]="0123456789abcdef";unsigned n=0,i,remaining;int r;
    if(log_fd<0)return 1;
    while(*tag&&n<130)text[n++]=*tag++;
    text[n++]=' ';text[n++]='0';text[n++]='x';
    for(i=0;i<16;i++)text[n++]=hex[(unsigned)(value>>((15-i)*4))&15];
    text[n++]='\n';
    remaining=n;i=0;while(remaining){r=sys_write(log_fd,text+i,remaining);if(r<=0||(unsigned)r>remaining)return 0;i+=(unsigned)r;remaining-=(unsigned)r;}return 1;
}
static int before(const char *tag){record(tag,0);if(log_fd>=0)sys_fsync(log_fd);OSTimeDly(1);return 1;}
static void finish(int result){status=result;done=1;if(registered)ui_exit_loop();}
static void error_record(const char *tag){
    if(log_fd<0)log_fd=sys_open("/mnt/card/omega-x7-error.txt",X7_O_WRONLY|X7_O_CREAT|X7_O_TRUNC,0600);
    record(tag,1);if(log_fd>=0)sys_fsync(log_fd);
}
static int fail(const char *tag){error_record(tag);finish(1);return -1;}
static unsigned crc32(const unsigned char *data,unsigned n){
    unsigned c=~0u,i,j;for(i=0;i<n;i++){c^=data[i];for(j=0;j<8;j++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;
}
static int load_rom(const char *name,unsigned char *dst,unsigned size,unsigned expected){
    char path[160]=OMEGA_ROM_DIR;unsigned n=0,i=0;int fd,r;struct x7_stat st;
    while(path[n])n++;
    while(name[i]&&n<sizeof(path)-1)path[n++]=name[i++];
    if(name[i])return fail("ROM path too long");
    path[n]=0;
    before(path);fd=sys_open(path,X7_O_RDONLY,0);record("ROM open result",(x7_s64)fd);if(fd<0)return fail("ROM open failed");
    before("BEFORE ROM fstat");memset(&st,0,sizeof(st));r=sys_fstat(fd,&st);record("ROM fstat result",(x7_s64)r);
    record("ROM file size",(x7_u64)st.st_size);
    if(r<0||st.st_size!=(x7_s64)size){sys_close(fd);return fail("ROM unexpected size or stat failure");}
    n=0;while(n<size){
        unsigned count=size-n;if(count>1024)count=1024;
        before("BEFORE ROM read");r=sys_read(fd,dst+n,count);record("ROM read result",(x7_s64)r);
        if(r<=0||(unsigned)r>count){sys_close(fd);return fail("ROM short read");}n+=(unsigned)r;
    }
    before("BEFORE ROM close");r=sys_close(fd);record("ROM close result",(x7_s64)r);if(r<0)return fail("ROM close failed");
    before("BEFORE ROM CRC");
    if(crc32(dst,size)!=expected)return fail("ROM CRC mismatch");
    before("ROM validated");
    return 0;
}
static void input_poll(void){
    unsigned records[10][3],now=OSTimeGet(),i;int n=sys_read(key_fd,records,sizeof(records));
    if(n>0){
        if(n>(int)sizeof(records)||n%12){fail("Invalid raw input size");return;}
        for(i=0;i<(unsigned)n/12;i++)if(!records[i][0]){
            unsigned next=records[i][1],j;
            for(j=0;j<2;j++){
                unsigned bit=4u<<j;
                if((next&bit)&&!(raw_state&bit))toggle_since[j]=now;
                if(!(next&bit)&&(raw_state&bit)&&!(filtered&bit))rejected++;
            }
            if((next&OMEGA_QUIT)&&!(raw_state&OMEGA_QUIT))quit_since=now;
            raw_state=next;filtered=(next&~12u)|(filtered&next&12u);
        }
    }
    for(i=0;i<2;i++){unsigned bit=4u<<i;if((raw_state&bit)&&now-toggle_since[i]>=(tick_rate+19)/20)filtered|=bit;}
    if((raw_state&OMEGA_QUIT)&&now-quit_since>=(tick_rate+1)/2)finish(0);
    angle_phase+=(now-angle_epoch)*40;angle_epoch=now;
    if(angle_phase>=tick_rate){
        unsigned updates=angle_phase/tick_rate;angle_phase%=tick_rate;
        if(updates>8)updates=8;
        if(filtered&OMEGA_LEFT)angle=(angle-OMEGA_TURN_STEP*updates)&63;
        if(filtered&OMEGA_RIGHT)angle=(angle+OMEGA_TURN_STEP*updates)&63;
    }
    omega_inputs(&machine,!!(filtered&OMEGA_FIRE),!!(filtered&OMEGA_THRUST),!!(filtered&OMEGA_COIN),!!(filtered&OMEGA_START),angle);
}
static void audio_pump(void){
    unsigned b,begin=OSTimeGet();int queued=0;
    if(!playing)return;
    for(b=0;b<10&&!done;b++){
        struct pcm pcm={{pcm_samples,pcm_samples,0,0,0,0},2,1024,0};
        if(audio_command(sound,0x4d414f45u,(x7_word_t)&queued)<0||queued<0){fail("Audio query failed");return;}
        audio_polls++;if((unsigned)queued>queue_max)queue_max=(unsigned)queued;
        queue_histogram[(unsigned)queued/1024<9?(unsigned)queued/1024:9]++;
        if(playing&&audio_blocks&&!queued)empty_audio++;
        if(queued>8192||machine.audio_write-machine.audio_read<1024)break;
        if(omega_audio_read(&machine,pcm_samples,1024)!=1024){fail("Audio ring short read");return;}
        if(audio_submit(sound,&pcm)<0){fail("Audio submit failed");return;}
        audio_blocks++;

    }
    audio_ticks+=OSTimeGet()-begin;
}
static void frame(void *arg){
    unsigned begin,i,target,available,needed;int queued=0;
    (void)arg;if(done||busy)return;busy=1;begin=OSTimeGet();input_poll();audio_pump();
    if(audio_command(sound,0x4d414f45u,(x7_word_t)&queued)<0||queued<0){fail("Audio query failed");busy=0;return;}
    /* Native sample consumption is the clock master. A wall-clock estimate
     * can drift against the DAC and eventually empty the queue. Supply about
     * 209 ms including samples still in our ring; never discard CPU time. */
    available=machine.audio_write-machine.audio_read;
    needed=(unsigned)queued+available<9216?9216-(unsigned)queued-available:0;
    target=machine.cycles+(needed*10000u+146u)/147u; /* 3 MHz / 44100 Hz */
    for(i=0;i<8&&!done;i++){
        int behind=(int)(target-machine.cycles);unsigned n,t;if(behind<=0)break;n=(unsigned)behind;if(n>15000)n=15000;
        t=OSTimeGet();omega_run(&machine,n);sim_ticks+=OSTimeGet()-t;audio_pump();
        if(OSTimeGet()-begin>=3)break;
    }
    draw_phase+=(OSTimeGet()-draw_epoch)*40;draw_epoch=OSTimeGet();
    if(!done&&draw_phase>=tick_rate){
        unsigned slot,t=OSTimeGet();int r=fb_video_acquire(&video,frames,&slot);
        if(r<0)fail("Framebuffer acquire failed");
        else if(!r){omega_render(&machine,fb_video_pixels(&video,slot));if(fb_video_submit(&video,slot)<0)fail("Framebuffer submit failed");else frames++;}
        draw_ticks+=OSTimeGet()-t;draw_phase%=tick_rate;
    }
    callbacks++;if(OSTimeGet()-begin>callback_max)callback_max=OSTimeGet()-begin;
    busy=0;
}
static void calibrate(void *arg){
    unsigned i,t;
    (void)arg;tick_rate=OSTimeGet()-cal_epoch;ui_kill_timer(cal_timer);cal_timer=-1;
    if(!tick_rate){fail("Clock calibration failed");return;}record("OS ticks per second",tick_rate);
    /* Prime emulation before native playback, yielding during startup. */
    for(i=0;i<20;i++){t=OSTimeGet();omega_run(&machine,30000);sim_ticks+=OSTimeGet()-t;OSTimeDly(1);}
    if(audio_command(sound,0x4d414f02u,0)<0){fail("Audio play failed");return;}
    playing=1;audio_pump();if(done)return;
    epoch=angle_epoch=draw_epoch=OSTimeGet();timer=ui_set_timer(1,frame,0);if(timer<0)fail("Frame timer failed");
}
static int window_message(struct ui_message *msg){if(msg->msgid==13)return 0;return ui_default_callback(msg);}
static void system_message(struct ui_app_message *msg){if(msg->type==1001)finish(0);}
static void cleanup(void){
    if(timer>=0)ui_kill_timer(timer);
    if(cal_timer>=0)ui_kill_timer(cal_timer);
    if(registered)ui_unregister_dispatcher(system_message);
    if(sound){audio_command(sound,0x4d414f03u,0);audio_close(sound);}
    /* No storage work inside the real-time callback. Save only after a normal
     * exit with playback stopped; failed startup cannot overwrite scores. */
    if(nvram_ready&&!status){
        unsigned i;for(i=0;i<OMEGA_NVRAM_SIZE;i++)if(machine.mem[0x5c00+i]!=saved_nvram[i])break;
        if(i<OMEGA_NVRAM_SIZE&&omega_nvram_save(machine.mem+0x5c00)<0)error_record("NVRAM save failed; previous file retained");
    }
    if(soundlib)x7_dlclose(soundlib);
    if(raw_mode)record("restore raw input",(x7_s64)sys_ioctl(key_fd,0x4003,0));
    if(key_fd>=0)sys_close(key_fd);
    if(mapping)record("restore key mapping",(x7_s64)x7_forbid_key_mapping(0));
    if(fb_video_close(&video,record,before)<0){error_record("Framebuffer cleanup failed");status=1;}
    if(window>0)ui_delete_window((unsigned)window);
    if(gui)x7_dlclose(gui);
    if(initialized)app_ui_quit();
    if(applib)x7_dlclose(applib);
    record("elapsed OS ticks",epoch?OSTimeGet()-epoch:0);record("main CPU cycles",machine.cycles);record("frames",frames);
    record("simulation OS ticks",sim_ticks);record("frame callbacks",callbacks);record("maximum callback OS ticks",callback_max);record("audio pump OS ticks",audio_ticks);record("audio queue polls",audio_polls);record("maximum reported queued frames",queue_max);
    {unsigned i;for(i=0;i<10;i++){record("audio queue bin lower bound",i*1024);record("audio queue bin count",queue_histogram[i]);}}
    record("CPU diagnostic errors",machine.cpu_errors);record("render and submit OS ticks",draw_ticks);
    record("vector lists",machine.vector_lists);record("last vector line count",machine.line_count);record("vector errors",machine.vector_errors);record("vector overflow",machine.vector_overflow);
    record("sound commands",machine.sound_commands);record("AY writes",machine.ay_writes);record("audio peak",machine.audio_peak);record("audio blocks",audio_blocks);record("empty audio queries",empty_audio);record("audio ring overflow",machine.audio_overflow);
    record("short toggle pulses rejected",rejected);record("watchdog reads",machine.watchdog);record("exit status",(unsigned)status);
    if(log_fd>=0){sys_fsync(log_fd);sys_close(log_fd);}app_exit(status);
}
void app_start_error(int stage,int result){log_fd=sys_open("/mnt/card/omega-x7-start-error.txt",X7_O_WRONLY|X7_O_CREAT|X7_O_TRUNC,0600);record("CRT stage",(unsigned)stage);record("status",(x7_s64)result);if(log_fd>=0){sys_fsync(log_fd);sys_close(log_fd);}log_fd=-1;}
void *process_start(void *arg){
    struct app_process_prefix *process;struct ui_app_message msg;unsigned argc=0;int format[3]={2,44100,0};void *libc_fs;(void)arg;
#if X7_OMEGA_DEBUG_LOG
    log_fd=sys_open("/mnt/card/omega-x7.txt",X7_O_WRONLY|X7_O_CREAT|X7_O_TRUNC,0600);
#endif
    before("Omega Race X7 v5; quarter-coverage antialiasing; persistent battery RAM; optional debug logging; instruction-based CPU; software vectors; dual Z80; integer AY audio");
    omega_init(&machine);process=app_get_process(app_getpid());if(!process){fail("No process");goto out;}
    if(process->argv)while(argc<32&&process->argv[argc])argc++;
    if(argc==32){fail("Launcher arguments invalid");goto out;}
    libc_fs=x7_dlopen("libc_fs.so",2);if(!libc_fs)libc_fs=x7_dlopen("libc_fs.so",2);if(!libc_fs){fail("libc_fs unavailable");goto out;}
    applib=x7_dlopen("/mnt/diska/lib/applib.so",1);if(!applib){fail("applib unavailable");goto out;}
    app_ui_init(argc,process->argv,0);initialized=1;mapping=1;x7_forbid_key_mapping(1);
    if(load_rom("omega.m7",machine.mem,4096,0x0424d46e)||load_rom("omega.l7",machine.mem+0x1000,4096,0xedcd7a7d)||load_rom("omega.k7",machine.mem+0x2000,4096,0x6d10f197)||load_rom("omega.j7",machine.mem+0x3000,4096,0x8e8d4b54)||load_rom("omega.e1",machine.mem+0x9000,2048,0x1d0fdf3a)||load_rom("omega.f1",machine.mem+0x9800,2048,0xd44c0814)||load_rom("sound.k5",machine.soundrom,2048,0x7d426017)||load_rom("dvgprom.bin",machine.prom,256,0xd481e958))goto out;
    {int r=omega_nvram_load(machine.mem+0x5c00);record("NVRAM load result",(x7_s64)r);if(r<0)error_record("NVRAM invalid or unreadable; using fresh battery RAM");}
    memcpy(saved_nvram,machine.mem+0x5c00,OMEGA_NVRAM_SIZE);nvram_ready=1;
    gui=x7_dlopen("gui.so",1);if(!gui){fail("GUI unavailable");goto out;}
    window=ui_create_window(0,0,480,272,2,window_message,0);if(window<=0){fail("Window failed");goto out;}ui_set_focus((unsigned)window);
    {void *dc=ui_dc_get((unsigned)window);if(!dc){fail("DC failed");goto out;}ui_background_color(dc,0);ui_clear_rect(dc,0,0,479,271);ui_screen_update();}
    if(fb_video_open(&video,record,before)<0){fail("Framebuffer failed");goto out;}
    key_fd=sys_open("/dev/key",X7_O_RDWR,0);if(key_fd<0){fail("Key driver unavailable");goto out;}
    raw_mode=1;if(sys_ioctl(key_fd,0x4003,1)<0){fail("Raw key mode failed");goto out;}
    soundlib=x7_dlopen("libsound.so",1);if(!soundlib){fail("Sound library unavailable");goto out;}
    sound=audio_open();if(!sound){fail("Audio open failed");goto out;}
    if(audio_command(sound,0x4d414f01u,(x7_word_t)format)<0||audio_command(sound,0x4d414f32u,2048)<0||audio_command(sound,0x4d414f33u,8)<0){fail("Audio setup failed");goto out;}
    ui_register_dispatcher(system_message);registered=1;before("BEFORE message loop");cal_epoch=OSTimeGet();cal_timer=ui_set_timer(1000,calibrate,0);if(cal_timer<0){fail("Calibration timer failed");goto out;}
    while(!done&&ui_get_msg(&msg)==1)ui_dispatch_msg(&msg);
out:cleanup();return 0;
}
