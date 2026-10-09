#include <assert.h>
#include <string.h>
#include "../ui_abi.h"
#ifdef PROBE_HSV
#include "../hsv.h"
static unsigned char pixels[HSV_HEIGHT][HSV_WIDTH];
static unsigned clears, yields;
#ifdef PROBE_BITMAP
static unsigned blits;
#endif
void OSTimeDly(x7_u16 ticks) {assert(ticks==1);yields++;}
#endif
static int scenario, deleted, destroyed, closed, killed, unregistered, exited, dispatched;
static int (*callback)(struct ui_message *);
static void (*timer_cb)(void *);
static int logfn(const char *s,x7_u64 v) {(void)s;(void)v;return 1;}
static int checkpoint(const char *s) {(void)s;return 1;}
void *x7_dlopen(const char *s,int f) {assert(strcmp(s,"gui.so")==0&&f==1);return scenario==5?0:(void*)1;}
char *x7_dlerror(void) {return "mock GUI loader failure";}
int x7_dlclose(void *p) {(void)p;closed++;return 0;}
x7_u32 OSTimeGet(void) {return 10;}
const char *ui_font_file(void) {return "font";}
int ui_create_font(const char *s,int n) {assert(strcmp(s,"/mnt/sdisk/ATTFV1.TTF")==0&&n==16);return scenario==6?-1:1;}
void ui_destroy_font(int n) {assert(n==1);destroyed++;}
int ui_default_fontface(int n) {assert(n==1);return scenario==7?-1:0;}
int ui_create_window(int x,int y,int w,int h,unsigned f,int (*cb)(struct ui_message*),unsigned p) {assert(x==0&&y==0&&f==2&&p==0);
#ifdef PROBE_HSV
assert(w==HSV_WIDTH&&h==HSV_HEIGHT);
#else
assert(w==240&&h==200);
#endif
callback=cb;return scenario==3?0:2;}
void ui_set_focus(unsigned w) {assert(w==2);}
void ui_delete_window(unsigned w) {assert(w==2);deleted++;}
void *ui_dc_get(unsigned w) {assert(w==2);return scenario==2?0:(void*)2;}
int ui_default_callback(struct ui_message *m) {(void)m;return 0;}
void ui_text_mode(void *d,int n) {assert(d==(void*)2&&n==2);}
void ui_font_size(void *d,int n) {assert(d==(void*)2&&n==16);}
static unsigned background;
void ui_color(void *d,unsigned n) {
#ifdef PROBE_FONT
 assert(d==(void*)2&&n==0xffffff);
#else
 (void)d;(void)n;assert(0 && "foreground not used for clear_rect");
#endif
}
void ui_background_color(void *d,unsigned n) {assert(d==(void*)2);background=n;}
void ui_clear_rect(void *d,int x,int y,int r,int b) {assert(d==(void*)2);
#ifdef PROBE_HSV
int row,col;assert(x>=0&&y>=0&&r<HSV_WIDTH&&b<HSV_HEIGHT&&r-x==3&&b-y==3);
assert(background==hsv_rgb((unsigned)x*1535/(HSV_WIDTH-HSV_TILE),(unsigned)y*255/(HSV_HEIGHT-HSV_TILE),255));
for(row=y;row<=b;row++)for(col=x;col<=r;col++){assert(!pixels[row][col]);pixels[row][col]=1;}
clears++;
#else
assert((x==0&&y==0&&r==239&&b==199&&background==0x102030)||(x==16&&y==16&&r==223&&b==183&&(background==0xff8000||background==0x00ff00)));
#endif
}
int ui_draw_bitmap(void *d,const void *data,int x,int y,int w,int h,int bpp) {
#ifdef PROBE_BITMAP
 const x7_u16 *pixels=data;
 unsigned row,col;
 assert(d==(void*)2&&data&&x==0&&y==0&&w==HSV_WIDTH&&h==HSV_HEIGHT&&bpp==2);
 assert(!blits++);
 for(row=0;row<HSV_HEIGHT;row++)for(col=0;col<HSV_WIDTH;col++)
  assert(pixels[row*HSV_WIDTH+col]==hsv_gui_pixel(hsv_rgb(col*1535/(HSV_WIDTH-1),row*255/(HSV_HEIGHT-1),255)));
 return 0;
#else
 (void)d;(void)data;(void)x;(void)y;(void)w;(void)h;(void)bpp;assert(0);return -1;
#endif
}
int ui_text(void *d,const char *s,const struct ui_rect *r,int a,int b) {assert(d==(void*)2&&(strcmp(s,"X7 font test")==0||strcmp(s,"START exits")==0)&&r->right==219&&a==1&&b==0);return 0;}
void ui_screen_update(void) {}
void ui_register_dispatcher(void (*f)(struct ui_app_message*)) {(void)f;}
void ui_unregister_dispatcher(void (*f)(struct ui_app_message*)) {(void)f;unregistered++;}
int ui_set_timer(unsigned ms,void (*cb)(void*),void *a) {assert(ms==20000&&a==0);timer_cb=cb;return scenario==4?-1:5;}
void ui_kill_timer(int n) {assert(n==5);killed++;}
void ui_exit_loop(void) {exited++;}
int ui_get_msg(struct ui_app_message *m) {m->type=0;return 1;}
void ui_dispatch_msg(struct ui_app_message *m) {
 struct ui_key k={8,0x20000000};struct ui_message g;
 (void)m;dispatched++;assert(dispatched==1);
 if(scenario==1)timer_cb(0);
 else {memset(&g,0,sizeof(g));g.msgid=13;g.data.key=&k;callback(&g);}
}
int main(int argc,char **argv) {
 int n; assert(argc==2);scenario=argv[1][0]-'0';
 n=ui_probe_run(logfn,checkpoint);
 assert(n==(scenario>=2?-1:0));assert(closed==(scenario==5?0:1));
 #ifdef PROBE_FONT
 assert(destroyed==((scenario==0||scenario==1||scenario==4||scenario==7)?1:0));
#else
 assert(destroyed==0);
#endif
 assert(deleted==(scenario==3||scenario==5?0:1));
 assert(unregistered==(scenario==2||scenario==3||scenario==5||scenario>=6?0:1));assert(killed==(scenario<2?1:0));
 assert(exited==(scenario<2?1:0));
#ifdef PROBE_HSV
 if(scenario<2||scenario==4){
  #ifdef PROBE_BITMAP
  assert(blits==1&&clears==0&&yields==8);
#else
  unsigned row,col;assert(clears==8160&&yields==68);
  for(row=0;row<HSV_HEIGHT;row++)for(col=0;col<HSV_WIDTH;col++)assert(pixels[row][col]==1);
 #endif
 }else assert(!clears);
#endif
 return 0;
}
