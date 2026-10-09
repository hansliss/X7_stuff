#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../ui_abi.h"
#include "../discovery_config.h"
static int scenario, initialized, quit, exits, loads, closes, getter;
static char *args[] = { "calculat.app", 0 };
static struct app_process_prefix process;
static char output[12000];
static unsigned used;
int app_getpid(void) {return 20;}
struct app_process_prefix *app_get_process(int pid) {assert(pid==20);process.argv=args;return &process;}
int sys_open(const char *p,int f,x7_u32 m) {(void)f;(void)m;assert(!strcmp(p,DISCOVERY_LOG));return 10;}
int sys_write(int fd,const void *p,x7_size_t n) {
 assert(fd==10&&used+n<sizeof(output));
 if(scenario==2&&initialized)return -5;
 memcpy(output+used,p,n);used+=n;output[used]=0;return n;
}
int sys_fsync(int fd) {assert(fd==10);return 0;}
int sys_close(int fd) {assert(fd==10);return 0;}
void OSTimeDly(x7_u16 ticks) {assert(ticks==1);}
x7_u32 OSTimeGet(void) {return 100;}
void *x7_dlopen(const char *p,int f) {
 if(!strcmp(p,"libc_fs.so")){assert(f==2);return (void*)1;}
 assert(f==1&&!initialized);
 if(!strcmp(p,"/mnt/diska/lib/applib.so"))return (void*)2;
 if(!strcmp(p,"gui.so")){loads++;return (void*)3;}
 if(scenario==1)return 0;
 loads++;return (void*)(unsigned long)(3+loads);
}
char *x7_dlerror(void) {return "mock candidate load failed";}
int x7_dlclose(void *p) {assert(p&&(!initialized||quit));closes++;return 0;}
void app_ui_init(int argc,char **argv,void *option) {
 assert(argc==1&&argv==args&&!option);
 assert(loads==(PROBE_DISCOVERY==5?5:2));initialized=1;
}
void app_ui_quit(void) {assert(initialized&&!quit);quit=1;}
const char *ui_font_file(void) {assert(initialized);getter++;return scenario==3?0:"attfv1.ttf";}
void app_exit(int status) {assert(status==0);exits++;}
int main(int argc,char **argv) {
 assert(argc==2);scenario=atoi(argv[1]);process_start(0);
 assert(exits==1&&closes==loads+1);
 if(scenario==1)assert(!initialized&&!quit&&!getter);
 else {assert(initialized&&quit);assert(getter==(scenario==2?0:1));}
 if(scenario==0)assert(strstr(output,"RETURNED default font getter")&&strstr(output,"attfv1.ttf")&&strstr(output,"DONE"));
 return 0;
}
