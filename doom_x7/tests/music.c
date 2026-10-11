/* Standalone sequencer/synth fixture. libc is used only by this host harness. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "x7_music.h"
void I_PlaySong(int,int);void I_PauseSong(int);void I_ResumeSong(int);
void I_ShutdownMusic(void);void I_StopSong(int);void I_UnRegisterSong(int);void I_SetMusicVolume(int);
void I_Error(char *fmt,...){(void)fmt;abort();}
int W_GetNumForName(char *name){(void)name;abort();}
void *W_CacheLumpNum(int lump,int tag){(void)lump;(void)tag;abort();}
int W_LumpLength(int lump){(void)lump;abort();}
int x7_record(const char *tag,unsigned long long value){
    if(!strcmp(tag,"music peak before master gain")){printf("Music peak: %llu before master gain\n",value);assert(value<=65534);}
    return 0;
}
static unsigned read32(const unsigned char *p){return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24;}
static unsigned run(unsigned frames){unsigned n,nonzero=0;for(n=0;n<frames;n++)nonzero+=x7_music_sample()!=0;return nonzero;}
int main(int argc,char **argv){
    FILE *f;unsigned char *wad,*bank=0;long size;unsigned count,directory,i,tracks=0;int h;
    assert(argc==2||argc==3);f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
    wad=malloc(size);assert(wad);assert(fread(wad,1,size,f)==(size_t)size);fclose(f);
    count=read32(wad+4);directory=read32(wad+8);
    for(i=0;i<count;i++){unsigned char *entry=wad+directory+16*i;if(!memcmp(entry+8,"GENMIDI",7)){bank=wad+read32(entry);assert(x7_music_set_bank(bank,read32(entry+4)));}}
    assert(bank);
    if(argc==3&&strcmp(argv[2],"synthetic")){
        unsigned long long energy=0;clock_t begin;unsigned frames=60*44100;
        for(i=0;i<count;i++){
            unsigned char *entry=wad+directory+16*i;
            if(memcmp(entry+8,"D_E1M1",6))continue;
            h=x7_music_register(wad+read32(entry),read32(entry+4));assert(h);I_PlaySong(h,1);I_SetMusicVolume(15);
            begin=clock();
            for(i=0;i<frames;i++){int value=x7_music_sample();energy+=(unsigned)(value<0?-value:value);}
            printf("60 audio seconds: %.6f CPU seconds; energy=%llu\n",(double)(clock()-begin)/CLOCKS_PER_SEC,energy);
            free(wad);return 0;
        }
        abort();
    }

    for(i=0;i<count;i++){
        unsigned char *entry=wad+directory+16*i;unsigned length=read32(entry+4);unsigned char *data=wad+read32(entry);
        if(argc==3||length<4||memcmp(data,"MUS\x1a",4))continue;
        h=x7_music_register(data,length);assert(h);I_PlaySong(h,1);I_SetMusicVolume(8);
        printf("Score %.8s\n",entry+8);fflush(stdout);
        assert(run(44100)>100);I_PauseSong(h);assert(run(1000)==0);I_ResumeSong(h);run(4410);
        I_SetMusicVolume(0);assert(run(1000)==0);I_SetMusicVolume(15);run(4410);
        /* Run through every score, including its end and looping restart.
         * OPL is still evaluated on every sample to exercise voice replacement. */
        {unsigned before=x7_music_loops(),seconds=0,j,duplicate=0;
         for(j=0;j<i;j++){unsigned char *prior=wad+directory+16*j;
             if(read32(prior+4)==length&&!memcmp(wad+read32(prior),data,length)){duplicate=1;break;}}
         if(!duplicate){
         while(x7_music_loops()==before&&seconds++<300)run(44100);
         assert(x7_music_loops()>before);}
         assert(x7_music_errors()==0);}
        I_UnRegisterSong(h);assert(run(1000)==0);tracks++;
    }
    /* Compare exact sample progression across a pause and verify 1/3 gain. */
    {unsigned char song[]={'M','U','S',26,7,0,16,0,1,0,0,0,0,0,0,0,0x90,188,100,0x81,0x0c,0x60,0x60};
     int reference[2000];unsigned j;h=x7_music_register(song,sizeof(song));assert(h);
     I_SetMusicVolume(15);I_PlaySong(h,0);
     for(j=0;j<2000;j++)reference[j]=x7_music_sample();
     I_PlaySong(h,0);for(j=0;j<1000;j++)assert(reference[j]==x7_music_sample());
     I_PauseSong(h);assert(run(1000)==0);I_ResumeSong(h);
     for(j=1000;j<2000;j++)assert(reference[j]==x7_music_sample());
     I_SetMusicVolume(5);I_PlaySong(h,0);
     for(j=0;j<2000;j++){int delta=x7_music_sample()*3-reference[j];assert(delta>=-3&&delta<=3);}
     I_StopSong(h);assert(run(1000)==0);
    }
    /* Synthetic controller/pitch/sustain and score-end cases, plus malformed data. */
    {unsigned char song[]={ 'M','U','S',26, 25,0,16,0, 1,0,0,0,0,0,0,0,
       0x40,0,3, 0x90,188,100,2, 0x20,150, 0x40,8,127, 0x80,60,2,
       0xc0,8,0,2, 0xc0,5,90,2, 0x60};
     song[4]=sizeof(song)-16;h=x7_music_register(song,sizeof(song));assert(h);I_PlaySong(h,0);assert(run(44100)>0);assert(x7_music_errors()==0);
     assert(!x7_music_register(song,15));assert(!x7_music_register(song,sizeof(song)-1));
     song[16]=0x50;h=x7_music_register(song,sizeof(song));assert(h);I_PlaySong(h,0);assert(run(1000)==0);assert(x7_music_errors()==1);
     song[4]=1;song[16]=0x60;h=x7_music_register(song,sizeof(song));assert(h);I_PlaySong(h,1);assert(run(1000)==0);assert(x7_music_errors()==2);
    }
    printf("Music: %u WAD scores rendered, distinct scores looped, paused, resumed and muted; bounds/controllers passed.\n",tracks);
    I_ShutdownMusic();free(wad);return 0;
}
