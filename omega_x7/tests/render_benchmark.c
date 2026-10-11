/* Replay actual-ROM line captures without CPU/audio emulation in the timing.
 * A baseline renderer may be linked as omega_render_baseline for comparison. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "machine.h"
#include "render.h"
#ifdef OMEGA_RENDER_BASELINE
void omega_render_baseline(const struct omega_machine *,uint16_t *);
#endif
static struct omega_machine m;
static uint16_t pixels[OMEGA_WIDTH*OMEGA_HEIGHT];
static void snapshot(const char *name,void (*render)(const struct omega_machine *,uint16_t *)){
    unsigned i;FILE *f=fopen(name,"wb");assert(f);render(&m,pixels);
    fprintf(f,"P6\n480 272\n255\n");
    for(i=0;i<OMEGA_WIDTH*OMEGA_HEIGHT;i++){
        unsigned p=pixels[i];unsigned char rgb[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));
}
static double bench(void (*render)(const struct omega_machine *,uint16_t *),unsigned repeats){
    unsigned i;clock_t begin=clock();for(i=0;i<repeats;i++)render(&m,pixels);
    return (double)(clock()-begin)/CLOCKS_PER_SEC;
}
int main(int argc,char **argv){
    FILE *f;unsigned repeats;double current;
    assert(argc==3||argc==4);f=fopen(argv[1],"r");assert(f);
    while(m.line_count<OMEGA_MAX_LINES){
        struct omega_line *l=&m.lines[m.line_count];
        if(fscanf(f,"%d %d %d %d %u",&l->x0,&l->y0,&l->x1,&l->y1,&l->intensity)!=5)break;
        m.line_count++;
    }
    assert(!fclose(f)&&m.line_count);repeats=(unsigned)strtoul(argv[2],0,10);assert(repeats);
    current=bench(omega_render,repeats);
    printf("%s: %u lines, %u frames, antialias CPU seconds %.6f",argv[1],m.line_count,repeats,current);
#ifdef OMEGA_RENDER_BASELINE
    {double baseline=bench(omega_render_baseline,repeats);printf(", baseline %.6f, ratio %.2fx",baseline,current/baseline);}
#endif
    putchar('\n');
    if(argc==4){
        char path[512];snprintf(path,sizeof(path),"%s-aa.ppm",argv[3]);snapshot(path,omega_render);
#ifdef OMEGA_RENDER_BASELINE
        snprintf(path,sizeof(path),"%s-before.ppm",argv[3]);snapshot(path,omega_render_baseline);
#endif
    }
    return 0;
}
