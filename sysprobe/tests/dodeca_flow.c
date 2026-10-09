#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../ui_abi.h"
#include "../dodeca_render.h"
static int scenario,blits,timers,kills,deleted,closed,registered,unregistered,exited;
static int (*window_cb)(struct ui_message *);
static void (*frame_cb)(void *),(*exit_cb)(void *);
static unsigned ticks;
static int record(const char *s,x7_u64 n){assert(s);(void)n;return 1;}
static int before(const char *s){assert(s);return 1;}
x7_u32 OSTimeGet(void){return ticks++;}
void OSTimeDly(x7_u16 n){assert(n==1);ticks++;}
void *x7_dlopen(const char *s,int flags){assert(!strcmp(s,"gui.so")&&flags==1);return scenario==2?0:(void*)1;}
int x7_dlclose(void *p){assert(p==(void*)1);closed++;return 0;}
int ui_create_window(int x,int y,int w,int h,unsigned f,int (*cb)(struct ui_message *),unsigned parent){assert(!x&&!y&&w==480&&h==272&&f==2&&!parent);window_cb=cb;return scenario==3?0:10;}
void ui_set_focus(unsigned w){assert(w==10);}
void *ui_dc_get(unsigned w){assert(w==10);return scenario==4?0:(void*)2;}
void ui_delete_window(unsigned w){assert(w==10);deleted++;}
int ui_default_callback(struct ui_message *p){(void)p;return 0;}
void ui_background_color(void *d,unsigned color){assert(d==(void*)2&&color==0x101828);}
void ui_clear_rect(void *d,int x,int y,int r,int b){assert(d==(void*)2&&!x&&!y&&r==479&&b==271);}
int ui_draw_bitmap(void *d,const void *p,int x,int y,int w,int h,int bpp){assert(d==(void*)2&&p&&x==DODECA_X&&y==DODECA_Y&&w==DODECA_WIDTH&&h==DODECA_HEIGHT&&bpp==2);blits++;return scenario==7?-1:0;}
void ui_screen_update(void){}
void ui_register_dispatcher(void (*cb)(struct ui_app_message *)){assert(cb);registered++;}
void ui_unregister_dispatcher(void (*cb)(struct ui_app_message *)){assert(cb);unregistered++;}
void ui_exit_loop(void){exited++;}
int ui_set_timer(unsigned ms,void (*cb)(void *),void *arg){
 assert(!arg);timers++;
 if(ms==10){frame_cb=cb;return scenario==5?-1:1;}
 assert(ms==30000);exit_cb=cb;return scenario==6?-1:2;
}
void ui_kill_timer(int t){assert(t==1||t==2);kills++;}
int ui_get_msg(struct ui_app_message *p){
 unsigned i;struct ui_key key={8,0x20000000};struct ui_message msg;
 (void)p;for(i=0;i<3;i++)frame_cb(0);
 if(scenario==1)exit_cb(0);
 else {memset(&msg,0,sizeof(msg));msg.msgid=13;msg.data.key=&key;window_cb(&msg);}
 /* Another pending frame must not draw after exit has been requested. */
 frame_cb(0);assert(blits==4);return 0;
}
void ui_dispatch_msg(struct ui_app_message *p){(void)p;assert(0);}
int main(int argc,char **argv){
 int status;assert(argc==2);scenario=atoi(argv[1]);status=ui_probe_run(record,before);
 assert(status==(scenario<2?0:-1));assert(closed==(scenario==2?0:1));assert(deleted==((scenario==2||scenario==3)?0:1));
 assert(unregistered==registered);
 assert(kills==(scenario<2?2:scenario==6?1:0));
 assert(exited==((scenario<2||scenario==7)?1:0));return 0;
}
