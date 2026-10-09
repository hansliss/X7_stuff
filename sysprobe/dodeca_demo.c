#include "ui_abi.h"
#include "dodeca_render.h"
#ifdef PROBE_FB
#include "fb_video.h"
static struct fb_video video;
static unsigned start_ticks,elapsed_ticks;
#ifdef PROBE_FAST
static unsigned measured_frames,loop_ticks,animation_epoch,animation_clock_started;
static unsigned frame_buckets[64],last_bucket,bucket_overflow;
#endif
#else
static x7_u16 pixels[DODECA_HEIGHT][DODECA_WIDTH];
#endif
static void *dc;
static volatile unsigned done,busy;
static unsigned frames,render_ticks,blit_ticks,update_ticks,reason,visible;
static int blit_error;
static int (*log_value)(const char *,x7_u64);
static void finish(unsigned why){reason=why;done=1;ui_exit_loop();}
static void render_frame(void *arg){
 x7_u32 t;unsigned phase=frames;
#ifdef PROBE_FB
 unsigned slot=frames;
#endif
 int n;(void)arg;
 if(done||busy||!dc)return;
 busy=1;
#ifdef PROBE_FB
 #ifdef PROBE_FAST
 n=fb_video_acquire(&video,frames,&slot);
 if(n<0){blit_error=n;busy=0;finish(5);return;}
 if(n>0){busy=0;return;}
#endif
 x7_u16 *pixels=fb_video_pixels(&video,slot);
#endif
 #ifdef PROBE_FAST
 if(animation_clock_started)phase=(OSTimeGet()-animation_epoch)/3;
 else phase=0;
#endif
 t=OSTimeGet();visible=dodeca_render((x7_u16 *)pixels,phase);render_ticks+=OSTimeGet()-t;
 #ifdef PROBE_FB
 t=OSTimeGet();n=fb_video_submit(&video,slot);blit_ticks+=OSTimeGet()-t;
#else
 t=OSTimeGet();n=ui_draw_bitmap(dc,pixels,DODECA_X,DODECA_Y,DODECA_WIDTH,DODECA_HEIGHT,2);blit_ticks+=OSTimeGet()-t;
 #endif
 if(n<0){blit_error=n;busy=0;finish(5);return;}
 #ifndef PROBE_FB
 t=OSTimeGet();ui_screen_update();update_ticks+=OSTimeGet()-t;
#endif
 frames++;
#ifdef PROBE_FAST
 if(animation_clock_started){
  unsigned bucket=(OSTimeGet()-animation_epoch)/100;
  if(bucket<64){frame_buckets[bucket]++;if(bucket>last_bucket)last_bucket=bucket;}
  else bucket_overflow++;
 }
#endif
 busy=0;
#ifdef PROBE_FAST
 if(frames>=12000)finish(4);
#else
 OSTimeDly(1);
 if(frames>=1200)finish(4);
#endif
}
static void animate(void *arg){
#ifdef PROBE_FAST
 /* Bounded batches keep the message pump and watchdog service responsive. */
 unsigned first=OSTimeGet(),i;
 for(i=0;i<4&&!done;i++){render_frame(arg);if(OSTimeGet()-first>=1)break;}
#else
 render_frame(arg);
#endif
}
static void timeout(void *arg){(void)arg;finish(2);}
static void system_message(struct ui_app_message *msg){if(msg->type==1001)finish(3);}
static int window_message(struct ui_message *msg){
 if(msg->msgid==13&&msg->data.key){
  unsigned val=msg->data.key->val,type=msg->data.key->type;
  log_value("KEY val",val);log_value("KEY type",type);
  if(val&8)finish(1);
  return 0;
 }
 return ui_default_callback(msg);
}
int ui_probe_run(int (*record)(const char *,x7_u64),int (*before)(const char *)){
 struct ui_app_message msg;void *gui=0;int window=0,frame_timer=-1,exit_timer=-1,registered=0,status=-1,terminal=1;
 log_value=record;
#ifdef PROBE_FB
 video.fd=-1;
#endif
 if(!before("BEFORE dlopen gui.so"))goto out;
 gui=x7_dlopen("gui.so",1);record("GUI handle",(x7_word_t)gui);if(!gui)goto out;
 #ifdef PROBE_FAST
 struct dodeca_scene large_scene=dodeca_default_scene;large_scene.focal_length=525;
 if(dodeca_init(&large_scene)<0)
#else
 if(dodeca_init(&dodeca_default_scene)<0)
#endif
 {record("invalid camera",0);goto out;}
 if(!before("BEFORE fullscreen dodecahedron window"))goto out;
 window=ui_create_window(0,0,DODECA_SCREEN_WIDTH,DODECA_SCREEN_HEIGHT,2,window_message,0);record("window",(x7_u64)(x7_s64)window);if(window<=0)goto out;
 ui_set_focus((unsigned)window);dc=ui_dc_get((unsigned)window);if(!dc)goto out;
 if(!before("BEFORE solid fullscreen background"))goto out;
 ui_background_color(dc,0x101828);
 ui_clear_rect(dc,0,0,DODECA_SCREEN_WIDTH-1,DODECA_SCREEN_HEIGHT-1);
 #ifdef PROBE_FB
 ui_screen_update();
 if(fb_video_open(&video,record,before)<0)goto out;
 record("fb frame bytes",DODECA_WIDTH*DODECA_HEIGHT*2);
 start_ticks=OSTimeGet();
#else
 record("bitmap update bytes",sizeof(pixels));
#endif
 record("bitmap update X",DODECA_X);record("bitmap update Y",DODECA_Y);
 if(!before("BEFORE first frame"))goto out;
 render_frame(0);record("first-frame visible faces",visible);record("first-frame render ticks",render_ticks);record("first-frame blit ticks",blit_ticks);
 if(done)goto out;
 ui_register_dispatcher(system_message);registered=1;
 #ifdef PROBE_FAST
 record("projection scale percent",175);
 if(!before("BEFORE frame timer 1ms bounded batches"))goto out;
 frame_timer=ui_set_timer(1,animate,0);
#else
 if(!before("BEFORE frame timer 10ms"))goto out;
 frame_timer=ui_set_timer(10,animate,0);
#endif
 record("frame timer",(x7_u64)(x7_s64)frame_timer);if(frame_timer<0)goto out;
 if(!before("BEFORE exit timer 30000ms"))goto out;
 exit_timer=ui_set_timer(30000,timeout,0);record("exit timer",(x7_u64)(x7_s64)exit_timer);if(exit_timer<0)goto out;
 if(!before("BEFORE animation loop"))goto out;
 #ifdef PROBE_FAST
 start_ticks=OSTimeGet();animation_epoch=start_ticks;animation_clock_started=1;
 measured_frames=frames;render_ticks=0;blit_ticks=0;
#endif
 while(!done){int n=ui_get_msg(&msg);if(n!=1){terminal=n;break;}ui_dispatch_msg(&msg);}
 #ifdef PROBE_FAST
 loop_ticks=OSTimeGet()-start_ticks;animation_clock_started=0;
 measured_frames=frames-measured_frames;
#endif
 if(terminal!=1)record("get_msg terminal",(x7_u64)(x7_s64)terminal);
 status=blit_error<0?-1:0;
out:
#ifdef PROBE_FB
 if(frames)elapsed_ticks=OSTimeGet()-start_ticks;
#endif
 if(frame_timer>=0){before("BEFORE kill frame timer");ui_kill_timer(frame_timer);}
 if(exit_timer>=0){before("BEFORE kill exit timer");ui_kill_timer(exit_timer);}
 if(registered)ui_unregister_dispatcher(system_message);
 #ifdef PROBE_FB
 if(fb_video_close(&video,record,before)<0)status=-1;
 record("animation elapsed ticks",elapsed_ticks);
#ifdef PROBE_FAST
 record("measured loop ticks",loop_ticks);record("measured loop frames",measured_frames);
 record("rotation phase interval ticks",3);record("frame bucket width ticks",100);
 if(measured_frames){unsigned i;for(i=0;i<=last_bucket;i++){
  record("frame bucket index",i);record("frame bucket count",frame_buckets[i]);
 }}
 record("frame bucket overflow frames",bucket_overflow);
 record("fb protected slot skips",video.avoided);
 record("fb no available buffer",video.unavailable);
 record("fb address query failures",video.query_failures);
#endif
 record("total fb submit ticks",blit_ticks);
#endif
 dc=0;if(window>0){before("BEFORE delete window");ui_delete_window((unsigned)window);}
 if(gui){before("BEFORE close GUI");record("GUI close",(x7_u64)(x7_s64)x7_dlclose(gui));}
 record("frames",frames);record("total render ticks",render_ticks);record("total blit ticks",blit_ticks);record("total update ticks",update_ticks);
 record("exit reason (1=start 2=timeout 3=quit 4=frame limit 5=blit error)",reason);record("blit error",(x7_u64)(x7_s64)blit_error);
 return status;
}
