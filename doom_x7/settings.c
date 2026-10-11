#include "x7_runtime.h"
#include "../syscalls.h"
#include "doomdef.h"
#include "doomstat.h"
#include "x7_paths.h"
extern int mouseSensitivity,showMessages,screenblocks,detailLevel,usegamma;
extern int snd_SfxVolume,snd_MusicVolume;
#define CONFIG X7_DOOM_CONFIG_PATH
#define TEMP CONFIG ".tmp"
static int ready;
static struct option {const char *name;int *value;unsigned min,max;} options[]={
    {"mouse_sensitivity",&mouseSensitivity,0,9},
    {"sfx_volume",&snd_SfxVolume,0,15},{"music_volume",&snd_MusicVolume,0,15},
    {"show_messages",&showMessages,0,1},{"screenblocks",&screenblocks,3,11},
    {"detaillevel",&detailLevel,0,1},{"usegamma",&usegamma,0,4}
};
#define COUNT (sizeof(options)/sizeof(options[0]))
void x7_settings_load(void){
    char text[2049],*line;unsigned used=0,i,loaded=0;int fd,version=0,values[COUNT],valid[COUNT]={0};
    ready=1;
    fd=sys_open(CONFIG,O_RDONLY,0);if(fd<0){x7_record("settings loaded entries",0);return;}
    while(used<sizeof(text)-1){int n=sys_read(fd,text+used,sizeof(text)-1-used);if(n<0){sys_close(fd);return;}if(!n)break;if((unsigned)n>sizeof(text)-1-used){sys_close(fd);return;}used+=n;}
    if(used==sizeof(text)-1){char extra;int n=sys_read(fd,&extra,1);if(n){sys_close(fd);x7_record("settings oversized or unreadable",1);return;}}
    sys_close(fd);text[used]=0;line=text;
    while(*line){
        char *end=line,*p,*name;unsigned value=0,digits=0;int malformed=0;
        while(*end&&*end!='\n')end++;
        if(*end)*end++=0;
        p=line;while(*p==' '||*p=='\t'||*p=='\r')p++;
        name=p;while(*p&&*p!=' '&&*p!='\t'&&*p!='\r')p++;
        if(!*p){line=end;continue;}*p++=0;
        while(*p==' '||*p=='\t')p++;
        while(*p>='0'&&*p<='9'){
            if(value>1000)malformed=1;
            if(!malformed)value=value*10+(*p-'0');
            digits++;p++;
        }
        while(*p==' '||*p=='\t'||*p=='\r')p++;
        if(!digits||malformed||(*p&&*p!='#')){line=end;continue;}
        if(!strcmp(name,"x7_config_version"))version=value==1;
        for(i=0;i<COUNT;i++)if(!strcmp(name,options[i].name)&&value>=options[i].min&&value<=options[i].max){values[i]=value;valid[i]=1;}
        line=end;
    }
    if(version)for(i=0;i<COUNT;i++)if(valid[i]){*options[i].value=values[i];loaded++;}
    x7_record("settings loaded entries",loaded);
}
void x7_settings_save(void){
    char text[512];unsigned used,i,done=0;int fd,result=0;
    if(!ready)return;
    used=snprintf(text,sizeof(text),"x7_config_version 1\n");
    for(i=0;i<COUNT;i++)used+=snprintf(text+used,sizeof(text)-used,"%s %d\n",options[i].name,*options[i].value);
    if(used>=sizeof(text)){x7_record("settings saved",0);return;}
    fd=sys_open(TEMP,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd>=0){
        while(done<used){int n=sys_write(fd,text+done,used-done);if(n<=0||(unsigned)n>used-done)break;done+=n;}
        result=done==used&&sys_fsync(fd)>=0;
        if(sys_close(fd)<0)result=0;
        if(result)result=sys_rename(TEMP,CONFIG)>=0;
    }
    x7_record("settings saved",result);
}
