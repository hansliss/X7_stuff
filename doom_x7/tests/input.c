/* Input translation checks with real engine types; no firmware emulation. */
#include <assert.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
boolean menuactive,messageToPrint,messageNeedsInput;
player_t players[MAXPLAYERS];int consoleplayer;
int key_up=KEY_UPARROW,key_down=KEY_DOWNARROW,key_left=KEY_LEFTARROW,key_right=KEY_RIGHTARROW;
int key_strafeleft=',',key_straferight='.',key_fire=KEY_RCTRL,key_use=' ',key_speed=KEY_RSHIFT;
static event_t captured[128];static unsigned count;
void D_PostEvent(event_t *e){assert(count<128);captured[count++]=*e;}
void x7_log_text(const char *s){(void)s;}
int x7_record(const char *s,unsigned long long n){(void)s;(void)n;return 0;}
void x7_input_snapshot(unsigned);void x7_input_key(unsigned,unsigned);void x7_input_tick(void);
void x7_input_raw_snapshot(unsigned,unsigned);void x7_input_raw_update(unsigned,unsigned);
static void expect(unsigned at,int key,evtype_t type){assert(captured[at].data1==key&&captured[at].type==type);}
int main(void){
    unsigned n;
    players[0].readyweapon=wp_pistol;players[0].pendingweapon=wp_nochange;
    players[0].weaponowned[wp_fist]=players[0].weaponowned[wp_pistol]=players[0].weaponowned[wp_shotgun]=true;
    x7_input_key(0x410,0x20000000);x7_input_tick();assert(count==2);
    x7_input_key(0x410,0x08000000);x7_input_tick();assert(count==2);
    x7_input_key(0x10,0x04000000);x7_input_tick();assert(count==3);expect(2,KEY_UPARROW,ev_keyup);
    menuactive=true;x7_input_tick();assert(count==4);expect(3,KEY_RCTRL,ev_keyup);
    x7_input_key(0x400,0x01000000);x7_input_tick();assert(count==4);
    x7_input_key(0x400,0x20000000);x7_input_tick();expect(4,KEY_ENTER,ev_keydown);
    x7_input_key(0x400,0x02000000);x7_input_tick();expect(5,KEY_ENTER,ev_keyup);
    messageToPrint=messageNeedsInput=true;x7_input_tick();
    x7_input_key(0x400,0x20000000);x7_input_tick();expect(6,'y',ev_keydown);
    x7_input_key(0x400,0x04000000);x7_input_tick();expect(7,'y',ev_keyup);
    menuactive=messageToPrint=messageNeedsInput=false;x7_input_tick();
    n=count;x7_input_key(0x2000,0x20000000);assert(count==n);
    x7_input_tick();assert(count==n+1);expect(n,'3',ev_keydown);
    x7_input_key(0x2000,0x10000000);assert(count==n+1);
    x7_input_tick();assert(count==n+2);expect(n+1,'3',ev_keyup);
    x7_input_key(0x2000,0x01000000);
    x7_input_key(0xc0,0x20000000);x7_input_tick();assert(count==n+4);
    expect(n+2,KEY_LEFTARROW,ev_keydown);expect(n+3,KEY_RIGHTARROW,ev_keydown);
    x7_input_key(0xc0,0x04000000);x7_input_tick();assert(count==n+6);
    n=count;x7_input_key(0x100,0x20000000);x7_input_tick();expect(n,',',ev_keydown);
    x7_input_key(0x100,0x04000000);x7_input_tick();expect(n+1,',',ev_keyup);
    n=count;x7_input_key(0x200,0x20000000);x7_input_tick();expect(n,'.',ev_keydown);
    x7_input_key(0x200,0x04000000);x7_input_tick();expect(n+1,'.',ev_keyup);
    n=count;x7_input_key(0x1800,0x20000000);x7_input_tick();assert(count==n+2);expect(n,' ',ev_keydown);expect(n+1,KEY_RSHIFT,ev_keydown);
    x7_input_key(0x1800,0x02000000);x7_input_tick();assert(count==n+4);
    n=count;x7_input_key(4,0x20000000);x7_input_tick();expect(n,KEY_TAB,ev_keydown);
    x7_input_key(4,0x08000000);x7_input_tick();assert(count==n+1);
    x7_input_key(4,0x04000000);x7_input_tick();expect(n+1,KEY_TAB,ev_keyup);
    n=count;x7_input_snapshot(0x100);x7_input_tick();expect(n,',',ev_keydown);
    x7_input_snapshot(0x110);x7_input_tick();expect(n+1,KEY_UPARROW,ev_keydown);
    x7_input_snapshot(0x510);x7_input_tick();expect(n+2,KEY_RCTRL,ev_keydown);
    x7_input_snapshot(0x410);x7_input_tick();expect(n+3,',',ev_keyup);
    x7_input_snapshot(0x400);x7_input_tick();expect(n+4,KEY_UPARROW,ev_keyup);
    x7_input_snapshot(0);x7_input_tick();expect(n+5,KEY_RCTRL,ev_keyup);
    /* Shoulder release crossing Start/Select must not open either UI. */
    n=count;x7_input_raw_snapshot(0x200,100);x7_input_tick();expect(n,'.',ev_keydown);
    x7_input_raw_snapshot(8,101);x7_input_tick();expect(n+1,'.',ev_keyup);
    x7_input_raw_snapshot(4,102);x7_input_raw_snapshot(0,103);
    x7_input_raw_update(110,5);x7_input_tick();assert(count==n+2);
    /* A stable press works even if the driver sends no held-state repeats. */
    n=count;x7_input_raw_snapshot(0x418,120);x7_input_tick();assert(count==n+2);
    x7_input_raw_update(124,5);x7_input_tick();assert(count==n+2);
    x7_input_raw_update(125,5);x7_input_tick();expect(n+2,KEY_ESCAPE,ev_keydown);
    x7_input_raw_update(130,5);x7_input_tick();assert(count==n+3);
    x7_input_raw_snapshot(0,131);x7_input_tick();assert(count==n+6);
    /* Unsigned clock wrap and an accepted Select release. */
    n=count;x7_input_raw_snapshot(4,0xfffffffeu);x7_input_raw_update(3,5);x7_input_tick();expect(n,KEY_TAB,ev_keydown);
    x7_input_raw_snapshot(0,4);x7_input_tick();expect(n+1,KEY_TAB,ev_keyup);
    return 0;
}
