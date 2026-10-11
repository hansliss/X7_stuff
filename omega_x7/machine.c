/* Omega Race board model. Hardware maps checked against MAME's BSD driver.
 * AY timing/envelope model adapted from floooh/chips (see vendor notices). */
#include <string.h>
#include "machine.h"
#include "ay_shapes.h"
static uint8_t main_mem_read(void *,uint16_t);
static void main_mem_write(void *,uint16_t,uint8_t);
static uint8_t sound_mem_read(void *,uint16_t);
static void sound_mem_write(void *,uint16_t,uint8_t);
static uint8_t main_port_in(z80 *,uint8_t);
static void main_port_out(z80 *,uint8_t,uint8_t);
static uint8_t sound_port_in(z80 *,uint8_t);
static void sound_port_out(z80 *,uint8_t,uint8_t);
void omega_cpu_error(void *ptr){((struct omega_machine *)ptr)->cpu_errors++;}
static const unsigned volumes[16]={0,41,59,86,126,187,264,440,519,840,1197,1527,2017,2602,3300,4096};
void omega_init(struct omega_machine *m){
    memset(m,0,sizeof(*m));
    z80_init(&m->maincpu);z80_init(&m->soundcpu);
    m->maincpu.userdata=m;m->soundcpu.userdata=m;
    m->maincpu.read_byte=main_mem_read;m->maincpu.write_byte=main_mem_write;
    m->maincpu.port_in=main_port_in;m->maincpu.port_out=main_port_out;
    m->soundcpu.read_byte=sound_mem_read;m->soundcpu.write_byte=sound_mem_write;
    m->soundcpu.port_in=sound_port_in;m->soundcpu.port_out=sound_port_out;
    m->sample_wait=69;
    m->input0=255;m->input1=0x33;m->outputs=0x40;
    m->ay[0].rng=m->ay[1].rng=1;
}
unsigned omega_gray(unsigned angle){angle&=63;return angle^(angle>>1)^63;}
void omega_inputs(struct omega_machine *m,unsigned fire,unsigned thrust,unsigned coin,unsigned start,unsigned angle){
    m->input0=255u&~((fire?0x40u:0u)|(thrust?0x20u:0u)|(coin?1u:0u));
    m->input1=0x33u|(start?0x40u:0u);m->angle=angle&63;
}
static unsigned word(struct omega_machine *m,unsigned pc){unsigned a=0x8000+((pc&4095)*2);return m->mem[a]|((unsigned)m->mem[a+1]<<8);}
/* Number of pulses from cascaded 7497 multipliers, including their rounding. */
static int delta(unsigned magnitude,unsigned fin){
    unsigned bit,n=0,mx=magnitude<<2;
    for(bit=0;bit<12;bit++)if(mx&(1u<<(11-bit)))n+=(fin+(1u<<bit))>>(bit+1);
    return (int)n;
}
void omega_vector_go(struct omega_machine *m){
    unsigned pc=0,stack[4]={0},sp=0,scale=0,steps,busy=0;int x=0,y=0;
    if(m->dvg_busy)return;
    m->line_count=0;m->vector_lists++;
    for(steps=0;steps<8192;steps++){
        unsigned a=word(m,pc++),op=a>>12,b=0,s,fin,intensity;int nx,ny,dx,dy;
        busy+=16;
        if(op<=10)b=word(m,pc++);
        if(op==11){m->dvg_busy=busy?busy:1;return;}
        if(op==12){if(sp==4){m->vector_errors++;break;}stack[sp++]=pc;pc=a&4095;continue;}
        if(op==13){if(!sp){m->vector_errors++;break;}pc=stack[--sp];continue;}
        if(op==14){pc=a&4095;continue;}
        if(op==10){x=b&4095;y=a&4095;scale=b>>12;continue;}
        if(op==15){
            b=(a&15)<<8;a&=0xff00;
            s=(scale+(((a&0x800)>>11)|(((b&0x800)^0x800)>>10)|((b&0x800)>>9)))&15;
            /* Intensity lives in the low byte of the original short word. */
            intensity=(word(m,pc-1)>>4)&15;a&=0xf00;b&=0xf00;
        }else{s=(scale+op)&15;intensity=b>>12;}
        fin=(2u<<s)&0x7ff;busy+=2*fin;
        dx=delta(b&1023,fin);dy=delta(a&1023,fin);
        if(b&1024)dx=-dx;
        if(a&1024)dy=-dy;
        nx=x+dx;ny=y+dy;
        if(intensity){
            if(m->line_count<OMEGA_MAX_LINES){
                struct omega_line *l=&m->lines[m->line_count++];
                if(!(m->outputs&0x40)){l->x0=1024-x;l->y0=y;l->x1=1024-nx;l->y1=ny;}
                else{l->x0=x;l->y0=1024-y;l->x1=nx;l->y1=1024-ny;}
                l->intensity=intensity;
            }else m->vector_overflow++;
        }
        x=nx&4095;y=ny&4095;
    }
    m->vector_errors++;m->dvg_busy=busy?busy:1;
}
static void ay_write(struct omega_machine *m,unsigned chip,unsigned port,unsigned data){
    static const uint8_t mask[16]={255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255};
    struct omega_ay *a=&m->ay[chip];unsigned r;
    if(!(port&1)){a->addr=(uint8_t)data;return;}
    if(a->addr>15)return;
    r=a->addr;a->reg[r]=(uint8_t)(data&mask[r]);m->ay_writes++;
    if(r<6)a->tone_period[r/2]=a->reg[r&~1u]|((unsigned)a->reg[(r&~1u)+1]<<8);
    if(r==13){a->env_index=0;a->env_hold=0;a->env_count=0;}
}
static void ay_tick(struct omega_machine *m){
    unsigned c,i;
    for(c=0;c<2;c++){
        struct omega_ay *a=&m->ay[c];
        for(i=0;i<3;i++)if(++a->tone_count[i]>=a->tone_period[i]){a->tone_count[i]=0;a->tone_bit[i]^=1;}
        if(++a->noise_count>=a->reg[6]){a->noise_count=0;a->noise_bit^=1;if(a->noise_bit)a->rng=(a->rng>>1)|(((a->rng^(a->rng>>3))&1)<<16);}
        if(m->clock_env){
            unsigned period=a->reg[11]|((unsigned)a->reg[12]<<8);
            if(++a->env_count>=period){
                a->env_count=0;
                if(!a->env_hold){a->env_index=(a->env_index+1)&31;if(a->env_index==31&&(!(a->reg[13]&8)||(a->reg[13]&1)))a->env_hold=1;}
            }
        }
    }
    m->clock_env^=1;
}
static void sample(struct omega_machine *m){
    unsigned c,i;int value=0;
    for(c=0;c<2;c++){
        struct omega_ay *a=&m->ay[c];
        for(i=0;i<3;i++)if((a->tone_bit[i]|((a->reg[7]>>i)&1))&&((a->rng&1)|((a->reg[7]>>(i+3))&1))){
            unsigned amp=a->reg[8+i];value+=(int)volumes[(amp&16)?ay_shapes[a->reg[13]][a->env_index]:(amp&15)];
        }
    }
    /* Integer DC blocker; six channels stay within signed 16-bit scale. */
    m->dc+=(value*256-m->dc)/512;value-=m->dc/256;
    if((unsigned)(value<0?-value:value)>m->audio_peak)m->audio_peak=(unsigned)(value<0?-value:value);
    if(m->audio_write-m->audio_read<OMEGA_AUDIO_RING)m->audio[m->audio_write++&(OMEGA_AUDIO_RING-1)]=value;
    else m->audio_overflow++;
}
static unsigned main_read(struct omega_machine *m,unsigned port){
    switch(port){
    case 8:omega_vector_go(m);return 0;
    case 9:m->watchdog++;m->last_watchdog=m->cycles;return 0;
    case 11:return m->dvg_busy?0x80:0;
    case 16:return 255;
    case 17:return m->input0;
    case 18:return m->input1;
    case 21:return omega_gray(m->angle)<<2;
    case 22:return omega_gray(0);
    case 23:return 0x3f; /* upright, coin play, one coin per credit */
    default:return 255;
    }
}
static void main_write(struct omega_machine *m,unsigned port,unsigned data){
    if(port==10)m->dvg_busy=0;
    else if(port==19)m->outputs=data;
    else if(port==20){m->sound_latch=data;m->sound_irq=1;m->sound_commands++;}
}
static uint8_t main_mem_read(void *ptr,uint16_t addr){
    struct omega_machine *m=ptr;
    if((addr<0x4c00)||(addr>=0x5c00&&addr<0x5d00)||(addr>=0x8000&&addr<0xa000))return m->mem[addr];
    return 255;
}
static void main_mem_write(void *ptr,uint16_t addr,uint8_t data){
    struct omega_machine *m=ptr;
    if((addr>=0x4000&&addr<0x4c00)||(addr>=0x5c00&&addr<0x5d00)||(addr>=0x8000&&addr<0x9000))m->mem[addr]=data;
}
static uint8_t sound_mem_read(void *ptr,uint16_t addr){
    struct omega_machine *m=ptr;
    return addr<0x1000?m->soundrom[addr&2047]:(addr<0x1400?m->soundram[addr&1023]:255);
}
static void sound_mem_write(void *ptr,uint16_t addr,uint8_t data){
    struct omega_machine *m=ptr;if(addr>=0x1000&&addr<0x1400)m->soundram[addr&1023]=data;
}
static uint8_t main_port_in(z80 *cpu,uint8_t port){return (uint8_t)main_read(cpu->userdata,port);}
static void main_port_out(z80 *cpu,uint8_t port,uint8_t data){main_write(cpu->userdata,port,data);}
static uint8_t sound_port_in(z80 *cpu,uint8_t port){
    struct omega_machine *m=cpu->userdata;
    if(!port){m->sound_irq=0;cpu->int_pending=0;return (uint8_t)m->sound_latch;}
    return 255;
}
static void sound_port_out(z80 *cpu,uint8_t port,uint8_t data){
    if(port<4)ay_write(cpu->userdata,(port&2)>>1,port&1,data);
}
void omega_run(struct omega_machine *m,unsigned cycles){
    while(cycles){
        unsigned n;
        /* Instructions execute atomically at their start. Retain remaining
         * T-states across calls and interleave the two CPUs/peripheral events.
         * This changes sub-instruction bus timing, not nominal clock rates. */
        if(!m->main_wait){
            unsigned before=(unsigned)m->maincpu.cyc;z80_step(&m->maincpu);
            m->main_wait=(unsigned)m->maincpu.cyc-before;
            if(!m->main_wait){m->cpu_errors++;m->main_wait=4;}
        }
        if(!m->sound_wait){
            unsigned before=(unsigned)m->soundcpu.cyc;
            if(m->sound_irq)z80_gen_int(&m->soundcpu,255);
            z80_step(&m->soundcpu);m->sound_wait=((unsigned)m->soundcpu.cyc-before)*2;
            if(!m->sound_wait){m->cpu_errors++;m->sound_wait=8;}
        }
        n=m->main_wait;if(m->sound_wait<n)n=m->sound_wait;
        if(24-m->clock24<n)n=24-m->clock24;
        if(12288-m->irq_count<n)n=12288-m->irq_count;
        if(m->sample_wait<n)n=m->sample_wait;
        if(cycles<n)n=cycles;
        m->cycles+=n;cycles-=n;m->main_wait-=n;m->sound_wait-=n;
        m->dvg_busy=m->dvg_busy>n?m->dvg_busy-n:0;
        m->irq_count+=n;
        if(m->irq_count==12288){m->irq_count=0;z80_gen_int(&m->maincpu,255);z80_gen_nmi(&m->soundcpu);}
        m->clock24+=n;if(m->clock24==24){m->clock24=0;ay_tick(m);}
        m->sample_phase+=OMEGA_AUDIO_HZ*n;m->sample_wait-=n;
        if(!m->sample_wait){m->sample_phase-=OMEGA_MAIN_HZ;sample(m);m->sample_wait=68+(m->sample_phase<1200);}
    }
}
unsigned omega_audio_read(struct omega_machine *m,int32_t *out,unsigned n){
    unsigned i,available=m->audio_write-m->audio_read;if(n>available)n=available;
    for(i=0;i<n;i++)out[i]=m->audio[m->audio_read++&(OMEGA_AUDIO_RING-1)];
    return n;
}
