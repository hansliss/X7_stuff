#ifndef OMEGA_MACHINE_H
#define OMEGA_MACHINE_H
#include <stdint.h>
#include "vendor/fastz80/z80.h"
#define OMEGA_MAIN_HZ 3000000u
#define OMEGA_AUDIO_HZ 44100u
#define OMEGA_MAX_LINES 4096
#define OMEGA_AUDIO_RING 16384
struct omega_line { int x0,y0,x1,y1; unsigned intensity; };
struct omega_ay {
    uint8_t reg[16],addr;
    unsigned tone_period[3],tone_count[3],tone_bit[3];
    unsigned noise_count,noise_bit,rng,env_count,env_index,env_hold;
};
struct omega_machine {
    z80 maincpu,soundcpu; unsigned main_wait,sound_wait,cpu_errors;
    uint8_t mem[65536],soundrom[2048],soundram[1024],prom[256];
    struct omega_ay ay[2];
    struct omega_line lines[OMEGA_MAX_LINES]; unsigned line_count;
    unsigned cycles,irq_count,main_irq,sound_irq,sound_nmi,clock24,clock_env;
    unsigned dvg_busy,vector_lists,vector_errors,vector_overflow,watchdog,last_watchdog;
    unsigned input0,input1,angle,outputs,sound_latch,sound_commands,ay_writes;
    unsigned sample_phase,sample_wait,audio_read,audio_write,audio_overflow,audio_peak;
    int32_t audio[OMEGA_AUDIO_RING],dc;
};
void omega_init(struct omega_machine *);
void omega_run(struct omega_machine *,unsigned cycles);
void omega_vector_go(struct omega_machine *);
unsigned omega_audio_read(struct omega_machine *,int32_t *,unsigned count);
void omega_inputs(struct omega_machine *,unsigned fire,unsigned thrust,unsigned coin,unsigned start,unsigned angle);
unsigned omega_gray(unsigned angle);
#endif
