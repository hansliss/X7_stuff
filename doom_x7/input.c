#include "x7_runtime.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "g_game.h"
#include "x7_controls.h"
extern boolean menuactive,messageToPrint,messageNeedsInput;
extern int key_up,key_down,key_left,key_right,key_strafeleft,key_straferight,key_fire,key_use,key_speed;
static unsigned held,blocked,native_events,down_count,up_count;
static unsigned char sent[256];
static int previous_context,weapon_pending;
static unsigned trace_mask[256],trace_type[256],trace_count,seen_mask;
static unsigned bit_press[32],bit_release[32];
static unsigned raw_state,toggle_since[2],toggle_rejected,toggle_accepted;
static void post(int key,int down){event_t e={down?ev_keydown:ev_keyup,key,0,0};D_PostEvent(&e);if(down)down_count++;else up_count++;}
static int weapon_key(void){
    player_t *p=&players[consoleplayer];unsigned n,base=p->pendingweapon<NUMWEAPONS?(unsigned)p->pendingweapon:(unsigned)p->readyweapon;
    static const int keys[NUMWEAPONS]={'1','2','3','4','5','6','7','1','3'};
    for(n=1;n<=NUMWEAPONS;n++){unsigned w=(base+n)%NUMWEAPONS;if(p->weaponowned[w])return keys[w];}
    return '1';
}
void x7_input_key(unsigned mask,unsigned type){
    unsigned press=type&0x38000000u,release=type&0x07000000u,new_bits=mask&~held;
    unsigned i;
    native_events++;seen_mask|=mask;
    for(i=0;i<32;i++)if(mask&(1u<<i)){if(release)bit_release[i]++;else if(press)bit_press[i]++;}
    if(trace_count<256){trace_mask[trace_count]=mask;trace_type[trace_count++]=type;}
    if(release){held&=~mask;blocked&=~mask;}
    else if(press){
        held|=mask;
        if((new_bits&X7_NEXT_WEAPON)&&!(blocked&X7_NEXT_WEAPON)&&!menuactive&&!messageToPrint){weapon_pending=weapon_key();}
    }
}
/* Driver records are complete states, not GUI press/release events. */
void x7_input_snapshot(unsigned mask){
    unsigned released=held&~mask,pressed=mask&~held;
    if(released)x7_input_key(released,0x04000000u);
    if(pressed)x7_input_key(pressed,0x20000000u);
}
/* Start/Select share the shoulder ADC ladder. Require a stable 50 ms press
 * before toggling UI; release and all gameplay controls remain immediate.
 * Check after draining a queue batch, so historical pulses cannot be accepted. */
void x7_input_raw_snapshot(unsigned mask,unsigned now){
    unsigned i;
    for(i=0;i<2;i++){
        unsigned bit=4u<<i;
        if((mask&bit)&&!(raw_state&bit))toggle_since[i]=now;
        if(!(mask&bit)&&(raw_state&bit)&&!(held&bit))toggle_rejected++;
    }
    raw_state=mask;
    x7_input_snapshot((mask&~12u)|(held&mask&12u));
}
void x7_input_raw_update(unsigned now,unsigned delay){
    unsigned i,mask=held;
    for(i=0;i<2;i++){
        unsigned bit=4u<<i;
        if((raw_state&bit)&&!(held&bit)&&now-toggle_since[i]>=delay){mask|=bit;toggle_accepted++;}
    }
    x7_input_snapshot(mask);
}
void x7_input_tick(void){
    unsigned char want[256]={0};unsigned k,active;int menu=menuactive||messageToPrint;
    if(menu!=previous_context){blocked|=held&X7_CONTEXT_RELEASE;previous_context=menu;}
    active=held&~blocked;
    /* Numeric weapon selection must survive command generation for a tic. */
    if(!menu&&weapon_pending)want[(unsigned)weapon_pending]=1;
    weapon_pending=0;
#define MAP(mask,key) do { if(active&(mask))want[(unsigned)(key)]=1; } while(0)
    if(menu){
        MAP(X7_MENU_UP,key_up);MAP(X7_MENU_DOWN,key_down);
        MAP(X7_MENU_LEFT,key_left);MAP(X7_MENU_RIGHT,key_right);
        MAP(X7_MENU_CLOSE,KEY_ESCAPE);
        MAP(X7_MENU_CONFIRM,(messageToPrint&&messageNeedsInput)?'y':KEY_ENTER);
        MAP(X7_MENU_BACK,(messageToPrint&&messageNeedsInput)?'n':KEY_ESCAPE);
    }else{
        MAP(X7_FORWARD,key_up);MAP(X7_BACKWARD,key_down);
        MAP(X7_TURN_LEFT,key_left);MAP(X7_TURN_RIGHT,key_right);
        MAP(X7_OPEN_MENU,KEY_ESCAPE);
        MAP(X7_STRAFE_LEFT,key_strafeleft);MAP(X7_STRAFE_RIGHT,key_straferight);
        MAP(X7_FIRE,key_fire);MAP(X7_USE,key_use);MAP(X7_RUN,key_speed);MAP(X7_AUTOMAP,KEY_TAB);
    }
    /* Release before pressing the new context's keys. Held keys stay held
     * without repeated toggles; a release from any native UP class clears it. */
    for(k=0;k<256;k++)if(sent[k]&&!want[k])post(k,0);
    for(k=0;k<256;k++)if(!sent[k]&&want[k])post(k,1);
    memcpy(sent,want,sizeof(sent));
}
/* Buffer during play; batch output on exit to avoid hundreds of fsyncs. */
void x7_input_stats(void){
    unsigned i,used=0;char text[2048];
    x7_record("input native events",native_events);x7_record("input key presses",down_count);x7_record("input key releases",up_count);
    x7_record("input short toggle pulses rejected",toggle_rejected);x7_record("input stable toggle presses accepted",toggle_accepted);
    x7_record("input seen mask",seen_mask);x7_record("input trace omitted events",native_events-trace_count);
    for(i=0;i<32;i++)if(seen_mask&(1u<<i)){
        used+=snprintf(text+used,sizeof(text)-used,"input bit 0x%08x press/repeat=%u release=%u\n",1u<<i,bit_press[i],bit_release[i]);
        if(used>sizeof(text)-128){x7_log_text(text);used=0;}
    }
    if(used)x7_log_text(text);
    used=0;
    for(i=0;i<trace_count;i++){
        used+=snprintf(text+used,sizeof(text)-used,"input trace %u mask=0x%08x type=0x%08x\n",i,trace_mask[i],trace_type[i]);
        if(used>sizeof(text)-128){x7_log_text(text);used=0;}
    }
    if(used)x7_log_text(text);
}
