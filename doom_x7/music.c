/* Bounded MUS sequencer and GENMIDI OPL2 player for the X7.
 * GENMIDI mapping follows Chocolate Doom (GPL-2.0-or-later); see vendor/README.md.
 * Timing and synthesis advance only as samples are submitted to the shared queue. */
#include "x7_runtime.h"
#include "x7_music.h"
#include "i_sound.h"
#include "i_system.h"
#include "w_wad.h"
#include "z_zone.h"
#include "vendor/rp2040-opl/emu8950.h"
#include "music_tables.h"
#define RATE 44100
#define VOICES 9
#define CHIP_RATE 49716
#define MUSIC_CHUNK 128
static OPL chip;
static int32_t native[MUSIC_CHUNK];
static unsigned native_position,native_count,resample_phase;
static int32_t previous_sample,current_sample;
static const unsigned char *bank,*score,*position,*end;
static int handle,playing,music_paused,looping,volume=8;
static unsigned delay,clock_phase,serial,music_events,notes,stolen,loops,errors,peak;
struct channel {unsigned program,velocity,volume,expression,sustain;int bend;};
struct voice {int active,held,channel,key;unsigned age,velocity,frequency,layer;const unsigned char *instrument;};
static struct channel channels[16];
static struct voice voices[VOICES];
static const unsigned operators[2][VOICES]={{0,1,2,8,9,10,16,17,18},{3,4,5,11,12,13,19,20,21}};
static unsigned le16(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static void reg(unsigned r,unsigned v){OPL_writeReg(&chip,r,v);}
static void release(unsigned i){reg(0xb0+i,voices[i].frequency>>8);voices[i].active=0;voices[i].held=0;}
static void reset(void){
    unsigned i;OPL_reset(&chip);reg(1,0x20);reg(8,0x40);
    native_position=native_count=0;
    memset(voices,0,sizeof(voices));memset(channels,0,sizeof(channels));
    for(i=0;i<16;i++){channels[i].velocity=127;channels[i].volume=100;channels[i].expression=127;}
}
int x7_music_set_bank(const unsigned char *data,unsigned length){
    if(!data||length<8+175*36||memcmp(data,"#OPL_II#",8))return 0;
    bank=data+8;return 1;
}
void I_InitMusic(void){
    static int initialized;
    if(!initialized){
        /* OPL_new initializes shared tables; the retained chip is static. */
        OPL *initial=OPL_new(CHIP_RATE*72,CHIP_RATE);
        if(!initial)I_Error("Music synthesizer initialization failed");
        OPL_delete(initial);OPL_reset(&chip);initialized=1;
    }
    if(!bank){int lump=W_GetNumForName("GENMIDI");
        if(!x7_music_set_bank(W_CacheLumpNum(lump,PU_STATIC),(unsigned)W_LumpLength(lump)))
            I_Error("Invalid GENMIDI instrument bank");
    }
}
static void level(unsigned i){
    struct voice *v=&voices[i];struct channel *c=&channels[v->channel];
    const unsigned char *p=v->instrument+4+16*v->layer;
    unsigned cv=c->volume*c->expression/127;
    unsigned loud=(volume_mapping_table[v->velocity]*2*(volume_mapping_table[cv]+1))>>9;
    unsigned attenuation=63-loud,mod=p[5]&63;
    reg(0x40+operators[1][i],attenuation|(p[11]&0xc0));
    if((p[6]&1)&&mod<attenuation)mod=attenuation;
    reg(0x40+operators[0][i],mod|(p[4]&0xc0));
}
static void frequency(unsigned i){
    struct voice *v=&voices[i];const unsigned char *p=v->instrument;
    int note=(le16(p)&1)?p[3]:(v->channel==15?60:v->key)+(short)le16(p+18+16*v->layer);
    int index;unsigned f,octave;
    while(note<0)note+=12;
    while(note>95)note-=12;
    index=64+32*note+channels[v->channel].bend;
    if(v->layer)index+=(int)(p[2]/2)-64;
    if(index<0)index=0;
    if(index<284)f=frequency_curve[index];
    else {octave=(unsigned)(index-284)/384;if(octave>7)octave=7;
        f=frequency_curve[(unsigned)(index-284)%384+284]|(octave<<10);}
    v->frequency=f;reg(0xa0+i,f&255);reg(0xb0+i,(f>>8)|0x20);
}
static void note_off(unsigned channel,unsigned key);
static void start_voice(unsigned chosen,unsigned channel,unsigned key,unsigned velocity,unsigned instrument,unsigned layer){
    unsigned i;struct voice *v=&voices[chosen];const unsigned char *p;
    if(v->active){stolen++;release(chosen);}
    v->active=1;v->held=1;v->channel=channel;v->key=key;v->layer=layer;
    v->age=++serial;v->velocity=velocity;v->instrument=bank+instrument*36;
    p=v->instrument+4+16*layer;
    for(i=0;i<2;i++){unsigned op=operators[i][chosen];const unsigned char *a=p+(i?7:0);
        reg(0x40+op,63);reg(0x20+op,a[0]);reg(0x60+op,a[1]);reg(0x80+op,a[2]);reg(0xe0+op,a[3]);}
    reg(0xc0+chosen,p[6]|0x30);level(chosen);frequency(chosen);notes++;
}
static void note_on(unsigned channel,unsigned key,unsigned velocity){
    unsigned i,chosen=0,oldest=~0u,instrument=channels[channel].program;
    if(!velocity){note_off(channel,key);return;}
    if(channel==15){if(key<35||key>81)return;instrument=128+key-35;}
    for(i=0;i<VOICES;i++){if(!voices[i].active){chosen=i;break;}if(voices[i].age<oldest){chosen=i;oldest=voices[i].age;}}
    start_voice(chosen,channel,key,velocity,instrument,0);
    /* DMX 1.9 adds the detuned layer only if a voice remains available. */
    if(le16(bank+instrument*36)&4)
        for(i=0;i<VOICES;i++)if(!voices[i].active){start_voice(i,channel,key,velocity,instrument,1);break;}
}
static void note_off(unsigned channel,unsigned key){
    unsigned i;for(i=0;i<VOICES;i++)if(voices[i].active&&voices[i].channel==(int)channel&&voices[i].key==(int)key){
        voices[i].held=0;if(!channels[channel].sustain)release(i);}
}
static void controller(unsigned channel,unsigned control,unsigned value){
    unsigned i;struct channel *c=&channels[channel];
    switch(control){
    case 0:c->program=value;break;
    case 3:c->volume=value;break;
    case 5:c->expression=value;break;
    case 8:c->sustain=value>=64;break;
    case 10:case 11:for(i=0;i<VOICES;i++)if(voices[i].active&&voices[i].channel==(int)channel)release(i);return;
    case 14:c->sustain=0;c->expression=127;c->bend=0;break;
    default:return; /* Mono output: pan and effects controllers have no effect. */
    }
    for(i=0;i<VOICES;i++)if(voices[i].active&&voices[i].channel==(int)channel){
        if(!voices[i].held&&!c->sustain)release(i);
        else {level(i);if(control==14)frequency(i);}
    }
}
static int byte_read(void){if(position>=end){errors++;playing=0;return -1;}return *position++;}
static void advance(void){
    unsigned budget=4096;
    while(playing&&!delay&&budget--){
        int descriptor=byte_read(),a,b;unsigned channel,type;
        if(descriptor<0)return;
        channel=descriptor&15;type=(descriptor>>4)&7;music_events++;
        if(type==6){if(looping){loops++;reset();position=score;}else playing=0;continue;}
        a=byte_read();if(a<0)return;
        switch(type){
        case 0:if(a>127)goto bad;note_off(channel,a);break;
        case 1:if(a&128){b=byte_read();if(b<0)return;if(b>127)goto bad;channels[channel].velocity=b;}
            note_on(channel,a&127,channels[channel].velocity);break;
        case 2:channels[channel].bend=a/2-64;
            {unsigned i;for(i=0;i<VOICES;i++)if(voices[i].active&&voices[i].channel==(int)channel)frequency(i);}break;
        case 3:if(a<10||a>14)goto bad;controller(channel,a,0);break;
        case 4:b=byte_read();if(b<0)return;if(a>9||b>127)goto bad;controller(channel,a,b);break;
        default:goto bad;
        }
        if(descriptor&128){unsigned n=0;do {b=byte_read();if(b<0)return;if(++n>4)goto bad;delay=(delay<<7)|(b&127);}while(b&128);}
    }
    if(playing&&!delay){bad:errors++;playing=0;}
}
int x7_music_register(void *data,int length){
    const unsigned char *p=data;unsigned start,size,instruments;
    playing=music_paused=0;score=position=end=0;
    if(!p||length<16||memcmp(p,"MUS\x1a",4))return 0;
    start=le16(p+6);size=le16(p+4);instruments=le16(p+12);
    if(start<16+2*instruments||start>(unsigned)length||!size||size>(unsigned)length-start)return 0;
    I_InitMusic();score=p+start;position=score;end=score+size;
    handle=handle==2147483647?1:handle+1;
    return handle;
}
/* The playable generated engine supplies the actual lump length. */
int I_RegisterSong(void *data){(void)data;return 0;}
void I_PlaySong(int h,int loop){if(h!=handle||!score)return;reset();position=score;delay=clock_phase=resample_phase=0;previous_sample=current_sample=0;looping=loop;music_paused=0;playing=1;}
void I_StopSong(int h){if(h!=handle)return;playing=music_paused=0;reset();}
void I_UnRegisterSong(int h){if(h!=handle)return;I_StopSong(h);score=position=end=0;}
void I_PauseSong(int h){if(h==handle)music_paused=1;}
void I_ResumeSong(int h){if(h==handle)music_paused=0;}
void I_SetMusicVolume(int v){volume=v<0?0:v>15?15:v;}
static int32_t native_sample(void){
    if(native_position==native_count){
        unsigned count;
        if(!delay)advance();
        if(!playing)return 0;
        /* Never render across a MUS tick: a group of events may change the
         * instrument or release a note at that boundary. */
        count=(CHIP_RATE-clock_phase+139)/140;
        if(count>MUSIC_CHUNK)count=MUSIC_CHUNK;
        OPL_calc_buffer_linear(&chip,native,count);
        native_position=0;native_count=count;
        clock_phase+=140*count;
        if(clock_phase>=CHIP_RATE){clock_phase-=CHIP_RATE;if(delay)delay--;}
    }
    /* Half-scale bounds the 32-bit interpolation even with nine additive voices. */
    return native[native_position++]/2;
}
int x7_music_sample(void){
    int value;unsigned absolute;
    if(!playing||music_paused)return 0;
    resample_phase+=CHIP_RATE;
    while(resample_phase>=RATE){
        resample_phase-=RATE;previous_sample=current_sample;
        current_sample=native_sample();
    }
    /* Mono interpolation has a compile-time divisor, unlike the previous
     * four-channel converter. Raise music gain fourfold; master gain stays 1/2. */
    value=(previous_sample*(int)(RATE-resample_phase)+current_sample*(int)resample_phase)/RATE;
    value=value*volume*2/15;
    absolute=(unsigned)(value<0?-value:value);if(absolute>peak)peak=absolute;
    return value;
}
unsigned x7_music_errors(void){return errors;}
unsigned x7_music_loops(void){return loops;}
void I_ShutdownMusic(void){
    playing=music_paused=0;score=position=end=0;
    x7_record("music events",music_events);x7_record("music note starts",notes);
    x7_record("music stolen voices",stolen);x7_record("music loops",loops);
    x7_record("music malformed scores",errors);x7_record("music peak before master gain",peak);
}
