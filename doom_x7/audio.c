#include "x7_runtime.h"
#include "../sysprobe/app_abi.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_system.h"
#ifdef X7_PLAYABLE
#include "x7_music.h"
#endif
#define CHANNELS 8
#define RATE 44100
#define BLOCK 1024
#define QUEUE_TARGET 8192
#define MAX_REFILL_BLOCKS 10
struct pcm { void *planes[6]; int channels,frames,precision; };
void *audio_open(void);
int audio_command(void *,x7_u32,x7_word_t);
int audio_submit(void *,struct pcm *);
void audio_close(void *);
struct sample { byte *data; unsigned count,rate; };
struct voice { struct sample *sample; x7_u64 phase; unsigned step,age; int handle,volume; };
static struct sample samples[NUMSFX];
static struct voice voices[CHANNELS];
static unsigned pitch_table[256],age,blocks,starts,steals,queue_max,audio_ticks,polls;
static int next_handle=1;
static unsigned gain_phase,previous_gain=32768;
static const unsigned gains[3]={32768,21845,16384};
static unsigned raw_peak[3],scaled_peak[3],raw_over[3],clipped[3],phase_blocks[3];
static unsigned last_pump,max_pump_gap,empty_queries,queue_histogram[10];
void x7_audio_set_tic(unsigned tic){
#ifdef X7_PLAYABLE
    (void)tic;gain_phase=0;
#else
    gain_phase=(tic/(8*35))%3;
#endif
}
unsigned x7_audio_gain_divisor(void){return gain_phase+2;}
static x7_s32 mix[BLOCK] __attribute__((aligned(32)));
static void *library,*output;
extern int numChannels;
char *sndserver_filename="";
static unsigned le16(byte *p){return p[0]|((unsigned)p[1]<<8);}
static unsigned le32(byte *p){return le16(p)|(le16(p+2)<<16);}
void I_SetChannels(void){numChannels=CHANNELS;}
int I_GetSfxLumpNum(sfxinfo_t *s){char name[16];if(s->link)s=s->link;snprintf(name,sizeof(name),"ds%s",s->name);return W_GetNumForName(name);}
void I_InitSound(void){
    unsigned i;
    int format[3]={2,RATE,0};
    pitch_table[128]=65536;
    for(i=129;i<256;i++)pitch_table[i]=(unsigned)(((x7_u64)pitch_table[i-1]*65892)>>16);
    for(i=128;i>0;i--)pitch_table[i-1]=(unsigned)(((x7_u64)pitch_table[i]*65181)>>16);
    /* Cache only independent DMX effects. Aliases reuse the same allocation. */
    for(i=1;i<NUMSFX;i++)if(!S_sfx[i].link){
        char name[16];int lump,len;byte *data;unsigned count,rate;
        snprintf(name,sizeof(name),"ds%s",S_sfx[i].name);lump=W_CheckNumForName(name);
        if(lump<0)continue;
        len=W_LumpLength(lump);if(len<8)I_Error("Short sound lump %s",name);
        data=malloc((unsigned)len);if(!data)I_Error("Sound allocation failed");
        W_ReadLump(lump,data);count=le32(data+4);rate=le16(data+2);
        if(le16(data)!=3||!count||count>(unsigned)len-8||rate<4000||rate>48000){free(data);I_Error("Invalid DMX sound %s",name);}
        samples[i].data=data;samples[i].count=count;samples[i].rate=rate;
    }
    for(i=1;i<NUMSFX;i++)if(S_sfx[i].link){int n=(int)(S_sfx[i].link-S_sfx);if(n<=0||n>=NUMSFX)I_Error("Invalid sound alias");samples[i]=samples[n];}
    for(i=1;i<NUMSFX;i++)S_sfx[i].data=samples[i].data;
    x7_record("BEFORE libsound load",0);library=x7_dlopen("libsound.so",1);if(!library)I_Error("libsound unavailable");
    x7_record("BEFORE audio open",0);output=audio_open();if(!output)I_Error("Audio open failed");
    if(audio_command(output,0x4d414f01u,(x7_word_t)format)<0||audio_command(output,0x4d414f32u,2048)<0||audio_command(output,0x4d414f33u,8)<0)I_Error("Audio setup failed");
    x7_record("BEFORE audio play",0);if(audio_command(output,0x4d414f02u,0)<0)I_Error("Audio play failed");
    x7_record("audio output rate",RATE);
    x7_record("audio queue target frames",QUEUE_TARGET);
}
static struct voice *find(int h){int i;for(i=0;i<CHANNELS;i++)if(voices[i].handle==h)return &voices[i];return 0;}
void I_UpdateSoundParams(int h,int vol,int sep,int pitch){
    struct voice *v=find(h);(void)sep;if(!v)return;
    if(vol<0)vol=0;
    if(vol>15)vol=15;
    if(pitch<0)pitch=0;
    if(pitch>255)pitch=255;
    v->volume=vol;v->step=(unsigned)(((x7_u64)(v->sample->rate*65536u/RATE)*pitch_table[pitch])>>16);
}
int I_StartSound(int id,int vol,int sep,int pitch,int priority){
    unsigned i,best=0;struct voice *v;(void)priority;
    if(id<=0||id>=NUMSFX||!samples[id].data)return -1;
    for(i=0;i<CHANNELS;i++){if(!voices[i].handle){best=i;break;}if(voices[i].age<voices[best].age)best=i;}
    v=&voices[best];if(v->handle)steals++;
    v->sample=&samples[id];v->phase=0;v->age=++age;
    if(next_handle==0x7fffffff)next_handle=1;
    v->handle=next_handle++;I_UpdateSoundParams(v->handle,vol,sep,pitch);starts++;return v->handle;
}
void I_StopSound(int h){struct voice *v=find(h);if(v)v->handle=0;}
int I_SoundIsPlaying(int h){return find(h)!=0;}
void I_UpdateSound(void){}
void I_SubmitSound(void){}
void x7_audio_pump(void){
    unsigned begin=OSTimeGet(),b,i,c;int queued=0;
    if(!output)return;
    if(last_pump && begin-last_pump>max_pump_gap)max_pump_gap=begin-last_pump;
    last_pump=begin;
    for(b=0;b<MAX_REFILL_BLOCKS;b++){
        struct pcm pcm={{mix,mix,0,0,0,0},2,BLOCK,0};
        polls++;
        if(audio_command(output,0x4d414f45u,(x7_word_t)&queued)<0||queued<0)I_Error("Audio queue query failed");
        if(blocks && !queued)empty_queries++;
        queue_histogram[(unsigned)queued/1024<9?(unsigned)queued/1024:9]++;
        if((unsigned)queued>queue_max)queue_max=queued;
        /* Queue includes the library's staged sector as well as driver data.
         * Keep about 186-209 ms available across long GUI/display callbacks;
         * one callback must be able to refill more than two blocks. */
        if(queued>QUEUE_TARGET)break;
        for(i=0;i<BLOCK;i++){
            int value=0;
#ifdef X7_PLAYABLE
            value=x7_music_sample();
#endif
            for(c=0;c<CHANNELS;c++){
                struct voice *v=&voices[c];unsigned index;
                if(!v->handle)continue;
                index=(unsigned)(v->phase>>16);
                if(index>=v->sample->count){v->handle=0;continue;}
                value+=((int)v->sample->data[8+index]-128)*v->volume*8;
                v->phase+=v->step;
            }
            {
                unsigned peak=(unsigned)(value<0?-value:value);
                int gain=(int)previous_gain+((int)gains[gain_phase]-(int)previous_gain)*(int)(i+1)/BLOCK;
                if(peak>raw_peak[gain_phase])raw_peak[gain_phase]=peak;
                if(value>32767||value< -32768)raw_over[gain_phase]++;
                /* Attenuate BEFORE clipping; changing gain after clipping would
                 * make already-distorted samples quieter rather than repair them. */
                value=(int)(((x7_s64)value*gain)>>16);
                peak=(unsigned)(value<0?-value:value);
                if(peak>scaled_peak[gain_phase])scaled_peak[gain_phase]=peak;
                if(value>32767||value< -32768)clipped[gain_phase]++;
            }
            if(value>32767)value=32767;
            if(value< -32768)value= -32768;
            /* precision is fractional bits above signed 16-bit scale.
             * LIBSOUND 0x4e409508 shifts by 16-precision with saturation;
             * 0x4e409998 clamps to +/-2^(15+precision) before that shift.
             * A preceding optional <<8 also updates precision to 8. */
            mix[i]=value;
        }
        if(audio_submit(output,&pcm)<0)I_Error("Audio submit failed");
        previous_gain=gains[gain_phase];phase_blocks[gain_phase]++;
        blocks++;
    }
    audio_ticks+=OSTimeGet()-begin;
}
void I_ShutdownSound(void){
    unsigned i;
#ifdef X7_PLAYABLE
    I_ShutdownMusic();
#endif
    if(output){x7_record("BEFORE audio stop",0);x7_record("audio stop result",(x7_s64)audio_command(output,0x4d414f03u,0));x7_record("BEFORE audio close",0);audio_close(output);output=0;x7_record("RETURNED audio close",0);}
    if(library){x7_record("sound library close",(x7_s64)x7_dlclose(library));library=0;}
    for(i=1;i<NUMSFX;i++)if(!S_sfx[i].link&&samples[i].data){free(samples[i].data);samples[i].data=0;}
    for(i=0;i<3;i++){
        x7_record("audio gain divisor",i+2);x7_record("audio gain blocks",phase_blocks[i]);
        x7_record("audio raw peak",raw_peak[i]);x7_record("audio raw overrange samples",raw_over[i]);
        x7_record("audio attenuated peak",scaled_peak[i]);x7_record("audio clipped samples",clipped[i]);
    }
    for(i=0;i<10;i++){x7_record("audio queue bin lower bound frames",i*1024);x7_record("audio queue bin observations",queue_histogram[i]);}
    x7_record("audio maximum pump gap OS ticks",max_pump_gap);
    x7_record("audio zero-queue observations (not proof of underrun)",empty_queries);
    x7_record("audio blocks submitted",blocks);x7_record("audio sound starts",starts);x7_record("audio stolen voices",steals);x7_record("audio queue polls",polls);x7_record("audio maximum queued frames",queue_max);x7_record("audio pump OS ticks",audio_ticks);
}
#ifndef X7_PLAYABLE
/* The effects-only demo keeps its original behavior. */
void I_InitMusic(void){} void I_ShutdownMusic(void){} void I_SetMusicVolume(int v){(void)v;}
void I_PauseSong(int h){(void)h;} void I_ResumeSong(int h){(void)h;}
int I_RegisterSong(void *p){(void)p;return 1;}
void I_PlaySong(int h,int loop){(void)h;(void)loop;} void I_StopSong(int h){(void)h;}
void I_UnRegisterSong(int h){(void)h;}

#endif
