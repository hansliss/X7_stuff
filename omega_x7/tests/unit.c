#include <assert.h>
#include <string.h>
#include "machine.h"
#include "render.h"
static struct omega_machine m;
static uint16_t guarded[480*272+2];
static uint16_t reference[480*272];
static void put(unsigned pc,unsigned value){m.mem[0x8000+pc*2]=(uint8_t)value;m.mem[0x8001+pc*2]=(uint8_t)(value>>8);}
int main(void){
    unsigned i,seen[64]={0};
    for(i=0;i<64;i++){
        unsigned a=omega_gray(i),b=omega_gray(i+1),d=a^b;
        assert(a<64&&!seen[a]);seen[a]=1;assert(d&&!(d&(d-1)));
    }
    omega_init(&m);omega_inputs(&m,1,1,1,1,65);
    assert(m.input0==0x9e&&m.input1==0x73&&m.angle==1);
    /* ROM writes ignored, RAM writes accepted by the actual CPU bus. */
    {const uint8_t prog[]={0x3e,0x55,0x32,0,0,0x32,0,0x40,0x76};memcpy(m.mem,prog,sizeof(prog));}
    omega_run(&m,100);assert(m.mem[0]==0x3e&&m.mem[0x4000]==0x55);
    omega_init(&m);
    put(0,0xa200);put(1,0x0200);put(2,0x9000);put(3,0xf040);put(4,0x9000);put(5,0xf440);put(6,0xb000);
    omega_vector_go(&m);assert(m.vector_errors==0&&m.line_count==2&&m.dvg_busy);
    assert(m.lines[0].x0==512&&m.lines[0].y0==512&&m.lines[0].x1==576&&m.lines[0].y1==512);
    assert(m.lines[1].x1==512&&m.lines[1].intensity==15);
    /* IM0 acknowledge of RST 38 totals 13 T-states (plus the current NOP). */
    omega_init(&m);m.maincpu.iff1=m.maincpu.iff2=1;z80_gen_int(&m.maincpu,255);
    z80_step(&m.maincpu);assert(m.maincpu.pc==0x38&&m.maincpu.cyc==17);
    /* Vector-ROM subroutine with RAM return address. */
    omega_init(&m);put(0,0xc800);put(1,0xb000);put(2048,0x9000);put(2049,0xf020);put(2050,0xd000);
    omega_vector_go(&m);assert(m.line_count==1&&m.lines[0].x1==32&&m.vector_errors==0);
    /* Bounded malformed lists cannot hang or walk off the return stack. */
    omega_init(&m);put(0,0xe000);omega_vector_go(&m);assert(m.vector_errors&&m.line_count==0);
    omega_init(&m);put(0,0xc000);omega_vector_go(&m);assert(m.vector_errors);
    /* Clipping leaves guards and side margins untouched. */
    omega_init(&m);m.line_count=3;
    m.lines[0]=(struct omega_line){-100,512,1200,512,15};
    m.lines[1]=(struct omega_line){512,-100,512,1200,15};
    m.lines[2]=(struct omega_line){-100,-100,-10,-10,15};
    guarded[0]=0x1234;guarded[480*272+1]=0x5678;omega_render(&m,guarded+1);
    assert(guarded[0]==0x1234&&guarded[480*272+1]==0x5678);
    for(i=0;i<480*272;i++)if(i%480<104||i%480>=376)assert(guarded[i+1]==0);
    /* Subpixel horizontal lines have complementary quarter weights rather
     * than snapping all endpoints to a single integer row. */
    {
        const uint16_t primary[]={0xffff,0xbdf7,0x8410,0x4208};
        const uint16_t adjacent[]={0,0x4208,0x8410,0xbdf7};
        unsigned y,x;
        for(y=0;y<4;y++){
            m.line_count=1;m.lines[0]=(struct omega_line){0,(int)y,1023,(int)y,15};
            omega_render(&m,guarded+1);
            for(x=104;x<376;x++){
                assert(guarded[1+x]==primary[y]);
                assert(guarded[1+480+x]==adjacent[y]);
            }
        }
    }
    /* All octants, endpoint reversal and transposition preserve coverage.
     * Include collapsed vectors and lines touching every playfield edge. */
    {
        const struct omega_line lines[]={
            {0,0,1023,1023,15},{0,1023,1023,0,15},
            {12,34,987,235,9},{12,987,235,34,6},
            {0,1023,1023,1023,15},{1023,0,1023,1023,15},
            {512,512,512,512,15},{500,501,501,502,15},
            {0,0,1023,345,0},{0,1023,1023,677,99}
        };
        unsigned n,x,y;
        for(n=0;n<sizeof(lines)/sizeof(lines[0]);n++){
            struct omega_line l=lines[n];m.line_count=1;m.lines[0]=l;
            omega_render(&m,reference);
            m.lines[0]=(struct omega_line){l.x1,l.y1,l.x0,l.y0,l.intensity};
            omega_render(&m,guarded+1);assert(!memcmp(reference,guarded+1,sizeof(reference)));
            if(l.x0!=l.x1||l.y0!=l.y1){
                m.lines[0]=(struct omega_line){l.y0,l.x0,l.y1,l.x1,l.intensity};
                omega_render(&m,guarded+1);
                /* Tie slopes use the same major axis; integer diagonals are
                 * covered by reversal above rather than transpose symmetry. */
                if(l.x1-l.x0!=l.y1-l.y0&&l.x1-l.x0!=l.y0-l.y1)
                    for(y=0;y<272;y++)for(x=0;x<272;x++)
                        assert(reference[y*480+x+104]==guarded[1+x*480+y+104]);
            }
            assert(guarded[0]==0x1234&&guarded[480*272+1]==0x5678);
            for(i=0;i<480*272;i++)if(i%480<104||i%480>=376)assert(guarded[i+1]==0);
        }
        /* Intensity and line order remain independent, with max blending. */
        m.line_count=2;m.lines[0]=(struct omega_line){12,34,987,235,15};m.lines[1]=m.lines[0];m.lines[1].intensity=3;
        omega_render(&m,reference);
        {struct omega_line l=m.lines[0];m.lines[0]=m.lines[1];m.lines[1]=l;}
        omega_render(&m,guarded+1);assert(!memcmp(reference,guarded+1,sizeof(reference)));
    }
    return 0;
}
