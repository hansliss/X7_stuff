#include "x7_runtime.h"
#ifdef X7_PLAYABLE
#include "x7_display.h"
#endif
#include "../sysprobe/ui_abi.h"
#include "../sysprobe/fb_video.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#ifdef X7_PLAYABLE
#define CALIBRATION_MS 1000
#else
#define CALIBRATION_MS 5000
#endif
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#ifdef X7_REALTIME_AUDIO
#include "s_sound.h"
#include "i_sound.h"
void x7_audio_pump(void);
void x7_audio_set_tic(unsigned);
unsigned x7_audio_gain_divisor(void);
#endif

#ifdef X7_PLAYABLE
void x7_input_key(unsigned,unsigned);
void x7_input_tick(void);
void x7_input_stats(void);
int x7_forbid_key_mapping(int);
static int mapping_forbidden;
static int raw_key_fd=-1,raw_key_mode,title_wait;
static unsigned raw_records,raw_combined,raw_ignored;
void x7_input_snapshot(unsigned);
void x7_input_raw_snapshot(unsigned,unsigned);
void x7_input_raw_update(unsigned,unsigned);
#endif
extern void D_ProcessEvents(void),D_DoAdvanceDemo(void),D_Display(void);
extern void G_BuildTiccmd(ticcmd_t *);
extern boolean advancedemo;
extern ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS];
static struct fb_video video={.fd=-1};
static void *gui,*applib;
static int log_fd=-1,window,frame_timer=-1,cal_timer=-1,dispatcher,app_initialized;
static int closing,done,reason,cal_done,busy,measuring,zone_ready;
static unsigned epoch,tick_rate=100,cal_ticks,cal_epoch,bench_epoch,elapsed;
static unsigned frames,rendered_frames,sim_ticks,draw_ticks,convert_ticks,acquire_ticks,submit_ticks;
static unsigned callbacks,max_callback_ticks,skipped,first_gametic;
static unsigned callbacks_over_30ms,callbacks_over_60ms,callbacks_over_120ms;
static unsigned short palette[256];
static unsigned mode, demo_number=1;
#ifdef X7_PLAYABLE
static char startup_log[8192];
static unsigned startup_log_used,startup_epoch,loading_reads,loading_yields,last_loading_yield;
static int log_buffering=1;
static void log_write(const char *s,unsigned n){
    while(n){int r=sys_write(log_fd,s,n);if(r<=0||(unsigned)r>n)return;s+=r;n-=r;}
    sys_fsync(log_fd);
}
static void log_flush(void){if(log_fd>=0&&startup_log_used)log_write(startup_log,startup_log_used);startup_log_used=0;}
#endif

void x7_log_text(const char *s){
    unsigned n;
    if(log_fd<0||measuring)return;
    n=strlen(s);
#ifdef X7_PLAYABLE
    if(log_buffering){
        while(n){unsigned room=sizeof(startup_log)-startup_log_used,take=n<room?n:room;
            memcpy(startup_log+startup_log_used,s,take);startup_log_used+=take;s+=take;n-=take;
            if(startup_log_used==sizeof(startup_log))log_flush();}
    }else log_write(s,n);
#else
    {unsigned tries=0;while(n&&tries++<32){int r=sys_write(log_fd,s,n);if(r<=0||(unsigned)r>n)return;s+=r;n-=r;}sys_fsync(log_fd);}
#endif
}
int x7_record(const char *tag,x7_u64 value){
    char text[224];int saved=measuring;
    if(log_fd<0)return 0;
    measuring=0;snprintf(text,sizeof(text),"%s 0x%08x%08x\n",tag,(unsigned)(value>>32),(unsigned)value);
    x7_log_text(text);measuring=saved;return log_fd>=0;
}
static int before(const char *tag){
#ifdef X7_PLAYABLE
    x7_record(tag,OSTimeGet()-startup_epoch);
#else
    x7_record(tag,0);
#endif
    OSTimeDly(1);return 1;
}
static void finish(int why){if(!done){reason=why;done=1;if(frame_timer>=0||cal_timer>=0)ui_exit_loop();}}
static void cleanup(int status) __attribute__((noreturn));
static void cleanup(int status){
    if(closing){app_exit(status);for(;;){}}
    closing=1;measuring=0;
#ifdef X7_PLAYABLE
    log_buffering=0;log_flush();
#endif
    if(frame_timer>=0){ui_kill_timer(frame_timer);frame_timer=-1;}
    if(cal_timer>=0){ui_kill_timer(cal_timer);cal_timer=-1;}
    #ifdef X7_REALTIME_AUDIO
    I_ShutdownSound();
    #endif
#ifdef X7_PLAYABLE
    if(raw_key_fd>=0){
        if(raw_key_mode)x7_record("restore key driver mode result",(x7_s64)sys_ioctl(raw_key_fd,0x4003,0));
        raw_key_mode=0;sys_close(raw_key_fd);raw_key_fd=-1;
    }
    x7_record("raw input records",raw_records);x7_record("raw combined states",raw_combined);x7_record("raw ignored controller records",raw_ignored);
    if(mapping_forbidden){x7_record("restore firmware key mapping result",(x7_s64)x7_forbid_key_mapping(0));mapping_forbidden=0;}
    x7_input_stats();
    if(players[consoleplayer].mo){x7_record("player x",(x7_s64)players[consoleplayer].mo->x);x7_record("player y",(x7_s64)players[consoleplayer].mo->y);}
    x7_record("player bullet ammo",players[consoleplayer].ammo[am_clip]);
    x7_record("player ready weapon",players[consoleplayer].readyweapon);
#endif
    I_ShutdownGraphics();
#ifdef X7_PLAYABLE
    if(reason!=4&&!status)M_SaveDefaults();
#endif
    if(reason==4)status=1;
    x7_record("benchmark mode (0=full 1=noblit 2=nodraw+noblit)",mode);
    x7_record("demo number",demo_number);
#ifdef X7_REALTIME_AUDIO
    x7_record("playback target tics per second",35);
#endif
    x7_record("calibration interval requested milliseconds",CALIBRATION_MS);
    x7_record("calibration elapsed OS ticks",cal_ticks);
    x7_record("estimated OS ticks per second",tick_rate);
    x7_record("measured elapsed OS ticks",elapsed);
    x7_record("completed simulation tics",(unsigned)gametic-first_gametic);
    x7_record("submitted frames",frames);
    x7_record("rendered frames",rendered_frames);
    x7_record("simulation ticks",sim_ticks);
    x7_record("display ticks including output",draw_ticks);
    x7_record("palette conversion ticks",convert_ticks);
    x7_record("buffer acquisition ticks",acquire_ticks);
    x7_record("buffer submission ticks",submit_ticks);
    x7_record("frame callbacks",callbacks);
    x7_record("maximum callback ticks",max_callback_ticks);
    x7_record("callbacks at least 30ms",callbacks_over_30ms);
    x7_record("callbacks at least 60ms",callbacks_over_60ms);
    x7_record("callbacks at least 120ms",callbacks_over_120ms);
    x7_record("skipped submissions",skipped);
    x7_record("fb protected candidates",video.avoided);
    x7_record("fb unavailable",video.unavailable);
    x7_record("fb query failures",video.query_failures);
    x7_record("zone free bytes at exit",zone_ready?Z_FreeMemory():0);
    x7_runtime_stats();x7_runtime_close();
    if(app_initialized){before("BEFORE applib_quit");app_ui_quit();x7_record("RETURNED applib_quit",0);}
    if(applib)x7_record("applib close",(x7_s64)x7_dlclose(applib));
    x7_record("exit reason (1=demo complete 2=Start 3=system 4=error)",reason);
    x7_record("exit status",status);x7_record("DONE",0);
    if(log_fd>=0){sys_close(log_fd);log_fd=-1;}
    app_exit(status);for(;;){}
}
void exit(int status){if(!reason)reason=status?4:1;cleanup(status);}
void I_Error(char *fmt,...){char s[512];va_list a;measuring=0;va_start(a,fmt);vsnprintf(s,sizeof(s),fmt,a);va_end(a);x7_log_text("ERROR: ");x7_log_text(s);x7_log_text("\n");reason=4;if(bench_epoch)elapsed=OSTimeGet()-bench_epoch;cleanup(1);}
void I_Quit(void){finish(2);}
int mb_used=2;
byte *I_ZoneBase(int *n){byte *p;*n=mb_used*1024*1024;before("BEFORE allocate DOOM zone");x7_record("zone bytes",*n);p=malloc(*n);if(!p)I_Error("Cannot allocate %i-byte zone",*n);x7_record("zone allocation CPU address",(size_t)p);return p;}
byte *I_AllocLow(int n){byte *p=calloc(1,n);if(!p)I_Error("Cannot allocate screen buffer");return p;}
ticcmd_t *I_BaseTiccmd(void){static ticcmd_t empty;return &empty;}
int I_GetTime(void){unsigned ticks=OSTimeGet()-epoch;return (ticks/tick_rate)*35+(ticks%tick_rate)*35/tick_rate;}
void I_WaitVBL(int n){
    unsigned ticks=(unsigned)(n*tick_rate+69)/70;
#ifdef X7_PLAYABLE
    /* The stock quit sound waits 105 VBLs. Keep its queue fed while waiting. */
    while(ticks--){x7_audio_pump();OSTimeDly(1);}
#else
    OSTimeDly(ticks);
#endif
}
void I_Init(void){
#ifdef X7_REALTIME_AUDIO
 I_InitSound();
#endif
}
void I_Tactile(int a,int b,int c){(void)a;(void)b;(void)c;}
void I_StartFrame(void){}
#ifdef X7_PLAYABLE
static void poll_raw_keys(void){
    /* Driver queue capacity is ten 12-byte records. A single bounded read
     * avoids starving audio/rendering if records arrive continuously. */
    unsigned records[10][3],i;int n;
    if(!raw_key_mode)return;
    n=sys_read(raw_key_fd,records,sizeof(records));
    if(n<=0){x7_input_raw_update(OSTimeGet(),(tick_rate+19)/20);return;} /* Empty queue returns -1. */
    if(n>(int)sizeof(records)||n%12)I_Error("Invalid raw key record size %d",n);
    for(i=0;i<(unsigned)n/12;i++){
        unsigned mask=records[i][1];raw_records++;
        if(records[i][0]){raw_ignored++;continue;} /* Built-in controller only. */
        if(mask&&(mask&(mask-1)))raw_combined++;
        /* Consume the wake press and its release: opening the menu must
         * not also activate its first item or leave a gameplay key held. */
        if(title_wait){
            if(title_wait==1&&mask){M_StartControlPanel();title_wait=2;}
            else if(title_wait==2&&!mask)title_wait=0;
            continue;
        }
        x7_input_raw_snapshot(mask,OSTimeGet());
        if(mask&0x4000)finish(2);
    }
    x7_input_raw_update(OSTimeGet(),(tick_rate+19)/20);
}
#endif
void I_StartTic(void){
#ifdef X7_PLAYABLE
    poll_raw_keys();
    x7_input_tick();
#endif
}
void I_UpdateNoBlit(void){}
void I_ReadScreen(byte *p){memcpy(p,screens[0],320*200);}
void I_SetPalette(byte *p){unsigned i;for(i=0;i<256;i++){unsigned r=gammatable[usegamma][p[3*i]],g=gammatable[usegamma][p[3*i+1]],b=gammatable[usegamma][p[3*i+2]];palette[i]=((r>>3)<<11)|((g>>2)<<5)|(b>>3);}}
void I_FinishUpdate(void){
    unsigned t,slot;int n;unsigned short *out;const byte *in=screens[0];
    if(done||mode)return;
    t=OSTimeGet();n=fb_video_acquire(&video,frames,&slot);acquire_ticks+=OSTimeGet()-t;
    if(n<0)I_Error("Framebuffer address query failed");
    if(n>0){skipped++;return;}
    out=fb_video_pixels(&video,slot);t=OSTimeGet();
    /* All five slots start black; only the game rectangle changes. */
#ifdef X7_PLAYABLE
    x7_display_convert(out,in,palette);
#else
    {unsigned y,x;for(y=0;y<200;y++){unsigned short *row=out+(y+36)*480+80;for(x=0;x<320;x++)row[x]=palette[*in++];}}
#endif
#if defined(X7_REALTIME_AUDIO) && !defined(X7_PLAYABLE)
    /* Small gain label in the upper black margin: 1/2, 1/3 or 1/4.
     * 5x7 bitmap digits, doubled; no native font dependency. */
    {
        static const unsigned char glyphs[5][7]={
            {4,12,4,4,4,4,14}, {1,2,2,4,8,8,16},
            {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
            {2,6,10,18,31,2,2}};
        unsigned ch,yy,xx,sy,sx,divisor=x7_audio_gain_divisor();
        for(yy=8;yy<26;yy++)for(xx=218;xx<260;xx++)out[yy*480+xx]=0;
        for(ch=0;ch<3;ch++)for(yy=0;yy<7;yy++)for(xx=0;xx<5;xx++)
            if(glyphs[ch<2?ch:divisor][yy]&(16u>>xx))
                for(sy=0;sy<2;sy++)for(sx=0;sx<2;sx++)
                    out[(10+yy*2+sy)*480+220+ch*12+xx*2+sx]=0xffff;
    }
#endif
    convert_ticks+=OSTimeGet()-t;t=OSTimeGet();n=fb_video_submit(&video,slot);submit_ticks+=OSTimeGet()-t;
    if(n<0)I_Error("Framebuffer submit failed");
    frames++;
}
static void system_message(struct ui_app_message *msg){if(msg->type==1001)finish(3);}
static int window_message(struct ui_message *msg){
    if(msg->msgid==13&&msg->data.key){
        unsigned v=msg->data.key->val,type=msg->data.key->type;
#ifdef X7_PLAYABLE
        if(raw_key_mode)return 0; /* Raw snapshots are the only input source. */
        if((v&0x4000)&&(type&0x20000000))finish(2);
        else x7_input_key(v,type);
#else
        if((v&8)&&(type&0x20000000))finish(2);
#endif
        return 0;
    }
    return ui_default_callback(msg);
}
void I_InitGraphics(void){
    void *dc;
    if(window)return;
    before("BEFORE dlopen gui.so");gui=x7_dlopen("gui.so",1);if(!gui)I_Error("gui.so unavailable");
    before("BEFORE create window");window=ui_create_window(0,0,480,272,2,window_message,0);if(window<=0)I_Error("Window creation failed");
    ui_set_focus(window);dc=ui_dc_get(window);if(!dc)I_Error("No window DC");
    ui_background_color(dc,0);ui_clear_rect(dc,0,0,479,271);ui_screen_update();
    if(fb_video_open(&video,x7_record,before)<0)I_Error("Framebuffer setup failed (requires verified 480x272 mode)");
    ui_register_dispatcher(system_message);dispatcher=1;
}
void I_ShutdownGraphics(void){
    if(dispatcher){ui_unregister_dispatcher(system_message);dispatcher=0;}
    if(fb_video_close(&video,x7_record,before)<0){reason=4;x7_record("framebuffer cleanup failed",1);}
    if(window>0){ui_delete_window(window);window=0;}
    if(gui){x7_record("GUI close",(x7_s64)x7_dlclose(gui));gui=0;}
}
void I_InitNetwork(void){static doomcom_t single;doomcom=&single;single.id=DOOMCOM_ID;single.ticdup=1;single.numnodes=single.numplayers=1;single.consoleplayer=0;netgame=false;}
void I_NetCmd(void){I_Error("Network unavailable");}
void x7_benchmark_begin(void){
    zone_ready=1;first_gametic=gametic;
#ifdef X7_REALTIME_AUDIO
    epoch=OSTimeGet();
#endif
bench_epoch=OSTimeGet();measuring=1;
    frames=rendered_frames=sim_ticks=draw_ticks=convert_ticks=acquire_ticks=submit_ticks=0;
}
void x7_loading_checkpoint(void){
    if(measuring)return;
#ifdef X7_PLAYABLE
    loading_reads++;
    /* Yield at least every 32 reads, or after 100 ms of elapsed OS time. */
    if(!(loading_reads%32)||OSTimeGet()-last_loading_yield>=10){OSTimeDly(1);loading_yields++;last_loading_yield=OSTimeGet();}
#else
    OSTimeDly(1);
#endif
}
void x7_benchmark_done(void){elapsed=OSTimeGet()-bench_epoch;finish(1);}
static void frame(void *arg){
    unsigned start=OSTimeGet(),i;(void)arg;
    if(done||busy)return;
#ifdef X7_PLAYABLE
    poll_raw_keys();
#endif
    if(done||!cal_done)return;
    busy=1;
#ifdef X7_REALTIME_AUDIO
    if(measuring)x7_audio_set_tic((unsigned)gametic-first_gametic);
    if(measuring && (unsigned)(gametic-first_gametic)>=(unsigned)I_GetTime()){x7_audio_pump();busy=0;return;}
#endif
    for(i=0;i<4&&!done;i++){
#ifdef X7_REALTIME_AUDIO
        if(measuring && (unsigned)(gametic-first_gametic)>=(unsigned)I_GetTime())break;
#endif
        unsigned t=OSTimeGet();int was_measuring=measuring;
        I_StartTic();D_ProcessEvents();
#ifdef X7_PLAYABLE
        maketic=gametic;
#endif
        G_BuildTiccmd(&netcmds[consoleplayer][maketic%BACKUPTICS]);
        if(advancedemo)D_DoAdvanceDemo();
        M_Ticker();G_Ticker();
        if(done)break;
        gametic++;maketic++;
        if(was_measuring)sim_ticks+=OSTimeGet()-t;
#ifdef X7_REALTIME_AUDIO
        x7_audio_set_tic((unsigned)gametic-first_gametic);
        S_UpdateSounds(players[consoleplayer].mo);
        x7_audio_pump();
#endif
        t=OSTimeGet();D_Display();if(measuring){draw_ticks+=OSTimeGet()-t;if(!nodrawers)rendered_frames++;}
        if(OSTimeGet()-start>=1)break;
    }
    {unsigned duration=OSTimeGet()-start;callbacks++;
    if(duration>max_callback_ticks)max_callback_ticks=duration;
    if(duration*1000>=tick_rate*30)callbacks_over_30ms++;
    if(duration*1000>=tick_rate*60)callbacks_over_60ms++;
    if(duration*1000>=tick_rate*120)callbacks_over_120ms++;}
    busy=0;
}
static void message_loop(void){struct ui_app_message msg;while(!done){int r=ui_get_msg(&msg);if(r!=1){if(!done)finish(3);break;}ui_dispatch_msg(&msg);}}
void x7_run_loop(void){
    /* D_DoomMain has prepared ga_playdemo. The worker runs one GUI message
     * loop covering calibration and playback, with no nested or restarted
     * get_msg loop after exit_msg_loop. */
}
static void calibrated(void *arg){
    (void)arg;if(cal_done||done)return;
    cal_ticks=OSTimeGet()-cal_epoch;tick_rate=(cal_ticks*1000+CALIBRATION_MS/2)/CALIBRATION_MS;
#ifdef X7_PLAYABLE
    x7_record("startup loading read checkpoints",loading_reads);
    x7_record("startup loading yield ticks",loading_yields);
    x7_record("startup elapsed OS ticks",OSTimeGet()-startup_epoch);
    log_buffering=0;log_flush();
#endif
    if(!tick_rate)I_Error("Clock calibration returned zero");
    epoch=OSTimeGet();cal_done=1;
#ifdef X7_PLAYABLE
    x7_benchmark_begin();
#endif
}
void app_start_error(int stage,int status){log_fd=sys_open("/mnt/card/doom-x7-start-error.txt",O_WRONLY|O_CREAT|O_TRUNC,0600);x7_record("CRT failure stage",stage);x7_record("status",(x7_s64)status);if(log_fd>=0)sys_close(log_fd);log_fd=-1;}
void *process_start(void *arg){
    struct app_process_prefix *process;unsigned argc=0;
    void *libc_fs;int config;char settings[2];
#ifdef X7_PLAYABLE
    char *args[]={"doom-x7",0};
#else
    char *args[]={"doom-x7","-timedemo","DEMO1","-noblit","-nodraw",0};
    static char demo_name[]="DEMO1";
#endif
    (void)arg;
#if !defined(X7_PLAYABLE) || X7_DOOM_DEBUG_LOG
#ifdef X7_PLAYABLE
    log_fd=sys_open("/mnt/card/doom-x7-play.txt"
#elif defined(X7_REALTIME_AUDIO)
    log_fd=sys_open("/mnt/card/doom-x7-audio.txt"
#else
    log_fd=sys_open("/mnt/card/doom-x7-benchmark.txt"
#endif
    ,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(log_fd<0){app_exit(1);return 0;}
#endif
#ifdef X7_PLAYABLE
    x7_log_text("X7 DOOM playable v13; stable Start/Select input; batched OPL2 music; fourfold music gain; faster startup; persistent settings; title then clean menu; raw driver input; mono effects; 363x272 scaled 4:3\n");
#elif defined(X7_REALTIME_AUDIO)
    x7_log_text("X7 DOOM normal-speed demo with mono sound effects v5 deeper audio queue; gain comparison; 320x200 unscaled\n");
#else
    x7_log_text("X7 silent DOOM timedemo prototype v1; 320x200 unscaled\n");
#endif
#ifdef X7_PLAYABLE
    startup_epoch=OSTimeGet();last_loading_yield=startup_epoch;
#endif
    process=app_get_process(app_getpid());if(!process)I_Error("No process structure");
    if(process->argv)while(argc<32&&process->argv[argc])argc++;
    if(argc==32)I_Error("Too many launcher arguments");
    before("BEFORE libc_fs load");libc_fs=x7_dlopen("libc_fs.so",2);if(!libc_fs)libc_fs=x7_dlopen("libc_fs.so",2);if(!libc_fs)I_Error("libc_fs unavailable");
    before("BEFORE applib load");applib=x7_dlopen("/mnt/diska/lib/applib.so",1);if(!applib)I_Error("applib unavailable");
    before("BEFORE applib_init");app_ui_init(argc,process->argv,0);app_initialized=1;
#ifdef X7_PLAYABLE
    /* Same scoped disable/restore sequence as the vendor emulator. Set the
     * cleanup flag first so even a subsequent initialization failure restores. */
    mapping_forbidden=1;
    x7_record("disable firmware key mapping result",(x7_s64)x7_forbid_key_mapping(1));
#endif
    #ifndef X7_PLAYABLE
    /* Optional two ASCII digits: output mode 1/2/3, demo 1/2/3. Default 11. */
    config=open("/mnt/card/doom-x7-mode.txt",O_RDONLY);
    if(config>=0){if(read(config,settings,2)==2&&settings[0]>='1'&&settings[0]<='3'&&settings[1]>='1'&&settings[1]<='3'){mode=settings[0]-'1';demo_number=settings[1]-'0';}close(config);}
    demo_name[4]='0'+demo_number;args[2]=demo_name;myargc=3+mode;myargv=args;
    #else
    (void)config;(void)settings;myargc=1;myargv=args;
    #endif
    x7_record("selected mode",mode);x7_record("selected demo",demo_number);
#ifdef X7_PLAYABLE
    x7_display_init();
    x7_record("display output width",X7_DISPLAY_WIDTH);x7_record("display output height",X7_DISPLAY_HEIGHT);
#endif
    I_InitGraphics();
#ifdef X7_PLAYABLE
    raw_key_fd=sys_open("/dev/key",O_RDWR,0);
    if(raw_key_fd<0)I_Error("Cannot open raw key driver");
    /* Mode 1 preserves full 12-byte states. The service reads only four
     * bytes, so it cannot dequeue a record in this mode. */
    raw_key_mode=1;
    {int result=sys_ioctl(raw_key_fd,0x4003,1);
     x7_record("set raw key driver mode result",(x7_s64)result);
     if(result<0)I_Error("Cannot select raw key mode");}
#endif
    epoch=OSTimeGet();before("BEFORE D_DoomMain");D_DoomMain();
    if(done)cleanup(0);
#ifdef X7_PLAYABLE
    if(advancedemo)D_DoAdvanceDemo();
    title_wait=1;
    x7_record("startup waits for key",1);
    x7_record("startup menu active",menuactive);x7_record("startup game state",gamestate);
#endif
    before("BEFORE calibration and frame timers");cal_epoch=OSTimeGet();
    cal_timer=ui_set_timer(CALIBRATION_MS,calibrated,0);if(cal_timer<0)I_Error("Calibration timer failed");
    frame_timer=ui_set_timer(1,frame,0);if(frame_timer<0)I_Error("Frame timer failed");
    message_loop();if(reason!=1&&bench_epoch)elapsed=OSTimeGet()-bench_epoch;
    measuring=0;
    cleanup(reason==4?1:0);
}
