#include "ui_abi.h"
/* LIBG1.SO 0x5146dfa0..0x5146e2c0: group 21 slots 4..7.
 * Same pointer in planes 0/1 duplicates one plane into stereo output.
 * Samples occupy signed 32-bit words, using the emulator's precision=0.
 * LIBSOUND.SO copies the 36-byte descriptor and consumes samples synchronously. */
struct pcm { void *planes[6]; int channels, frames, precision; };
_Static_assert(sizeof(struct pcm) == 36, "PCM descriptor");
void *audio_open(void);
int audio_command(void *, x7_u32, x7_word_t);
int audio_submit(void *, struct pcm *);
void audio_close(void *);
static int (*log_value)(const char *, x7_u64);
static int (*mark)(const char *);
static void *sound;
static x7_s32 samples[1024] __attribute__((aligned(32)));
static unsigned start_ticks, blocks, phase, queue_max, polls, failures;
static int done, reason;
static void finish(int why) { if (!done) { done=1; reason=why; ui_exit_loop(); } }
static void system_message(struct ui_app_message *m) { if (m->type==1001) finish(3); }
static int window_message(struct ui_message *m)
{
    if (m->msgid==13 && m->data.key && (m->data.key->val & 8) &&
        (m->data.key->type & 0x20000000u)) { finish(1); return 0; }
    return ui_default_callback(m);
}
static void pump(void *arg)
{
    int queued=0, r;
    unsigned i, step;
    struct pcm pcm={{samples,samples,0,0,0,0},2,1024,0};
    (void)arg;
    if (done) return;
    if ((unsigned)(OSTimeGet()-start_ticks)>=1000) { finish(2); return; }
    if (blocks>=176) { /* allow queue to drain before the automatic exit */
        if ((unsigned)(OSTimeGet()-start_ticks)>=650) finish(2);
        return;
    }
    r=audio_command(sound,0x4d414f45u,(x7_word_t)&queued);
    ++polls;
    if (r<0 || queued<0) {
        log_value("queue query error", (x7_s64)r); ++failures; finish(4); return;
    }
    if ((unsigned)queued>queue_max) queue_max=(unsigned)queued;
    if (queued>2048) return; /* emulator's own queue threshold */
    /* ~440, ~660, ~880 Hz; half-second silent gaps. 1/16 full scale.
     * Triangle wave avoids harsh square-wave harmonics. */
    step=blocks<59 ? 653 : blocks<118 ? 981 : 1308;
    for (i=0;i<1024;++i) {
        int v;
        phase=(phase+step)&65535u;
        v=phase<32768 ? (int)phase : 65535-(int)phase;
        v=(v-16384)/8;
        if ((blocks>=44 && blocks<59) || (blocks>=103 && blocks<118) || blocks>=162) v=0;
        samples[i]=v;
    }
    r=audio_submit(sound,&pcm);
    if (!blocks) log_value("first submit result", (x7_s64)r);
    if (r<0) { log_value("submit error", (x7_s64)r); ++failures; finish(4); return; }
    ++blocks;
}
int ui_probe_run(int (*record)(const char *,x7_u64),int (*before)(const char *))
{
    void *gui=0,*library=0,*dc;
    int window=-1,timer=-1,registered=0,status=-1,r;
    int format[3]={2,44100,0};
    struct ui_app_message msg;
    log_value=record; mark=before;
    gui=x7_dlopen("gui.so",1); record("gui handle",(x7_word_t)gui);
    if (!gui) goto out;
    window=ui_create_window(0,0,480,272,2,window_message,0);
    record("window",(x7_s64)window); if (window<=0) goto out;
    ui_set_focus((unsigned)window); dc=ui_dc_get((unsigned)window);
    if (dc) { ui_background_color(dc,0x1800); ui_clear_rect(dc,0,0,480,272); ui_screen_update(); }
    if (!mark("BEFORE dlopen libsound.so")) goto out;
    library=x7_dlopen("libsound.so",1); record("libsound handle",(x7_word_t)library);
    if (!library) goto out;
    if (!mark("BEFORE audio open")) goto out;
    sound=audio_open(); record("audio handle",(x7_word_t)sound); if (!sound) goto out;
    if (!mark("BEFORE format stereo 44100 precision 0")) goto out;
    r=audio_command(sound,0x4d414f01u,(x7_word_t)format); record("format result",(x7_s64)r); if (r<0) goto out;
    r=audio_command(sound,0x4d414f32u,2048); record("sector size result",(x7_s64)r); if (r<0) goto out;
    r=audio_command(sound,0x4d414f33u,8); record("sector count result",(x7_s64)r); if (r<0) goto out;
    if (!mark("BEFORE audio play")) goto out;
    r=audio_command(sound,0x4d414f02u,0); record("play result",(x7_s64)r); if (r<0) goto out;
    ui_register_dispatcher(system_message); registered=1;
    start_ticks=OSTimeGet(); timer=ui_set_timer(20,pump,0);
    record("pump timer",(x7_s64)timer); if (timer<0) goto out;
    before("BEFORE message loop");
    while (!done && ui_get_msg(&msg)==1) ui_dispatch_msg(&msg);
    record("elapsed OS ticks",OSTimeGet()-start_ticks);
    record("blocks submitted",blocks); record("frames submitted",blocks*1024);
    record("queue polls",polls); record("maximum queue",queue_max);
    record("exit reason (1=start 2=timer 3=quit 4=error)",reason);
    status=failures ? -1 : 0;
out:
    if (timer>=0) ui_kill_timer(timer);
    if (registered) ui_unregister_dispatcher(system_message);
    if (sound) {
        before("BEFORE audio stop"); record("stop result",(x7_s64)audio_command(sound,0x4d414f03u,0));
        before("BEFORE audio close"); audio_close(sound); sound=0; record("RETURNED audio close",0);
    }
    if (library) { before("BEFORE dlclose libsound"); record("sound dlclose",(x7_s64)x7_dlclose(library)); }
    if (window>0) ui_delete_window((unsigned)window);
    if (gui) x7_dlclose(gui);
    record("audio probe status",(x7_s64)status); return status;
}
