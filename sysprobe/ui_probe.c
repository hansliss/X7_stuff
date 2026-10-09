#include "ui_abi.h"
#ifdef PROBE_HSV
#include "hsv.h"
#endif
static int (*log_value)(const char *, x7_u64);
static void *dc;
#ifdef PROBE_FONT
static int font_id = -1;
static int font_ready;
#endif
static volatile unsigned done;
static unsigned keys, messages, last_key, last_type, reason;
#ifdef PROBE_BITMAP
static x7_u16 bitmap_pixels[HSV_HEIGHT][HSV_WIDTH];
#endif
static void draw(void)
{
    if (!dc) return;
#ifdef PROBE_HSV
    {
        unsigned x, y;
        x7_u32 start_ticks, elapsed;
        log_value("BEFORE HSV map", 0);
        start_ticks = OSTimeGet();
#ifdef PROBE_BITMAP
        for (y = 0; y < HSV_HEIGHT; ++y) {
            unsigned saturation = y * 255 / (HSV_HEIGHT - 1);
            for (x = 0; x < HSV_WIDTH; ++x) {
                unsigned hue = x * 1535 / (HSV_WIDTH - 1);
                bitmap_pixels[y][x] = hsv_gui_pixel(hsv_rgb(hue, saturation, 255));
            }
            /* Eight yields total instead of one for every scanline. */
            if ((y & 31) == 31) OSTimeDly(1);
        }
        elapsed = OSTimeGet() - start_ticks;
        log_value("bitmap generation ticks", elapsed);
        log_value("bitmap bytes", sizeof(bitmap_pixels));
        log_value("BEFORE bitmap blit", 0);
        start_ticks = OSTimeGet();
        {
            int result = ui_draw_bitmap(dc, bitmap_pixels, 0, 0, HSV_WIDTH, HSV_HEIGHT, 2);
            elapsed = OSTimeGet() - start_ticks;
            log_value("bitmap blit return (semantics unconfirmed)", (x7_u64)(x7_s64)result);
            log_value("bitmap blit ticks", elapsed);
        }
#else
        for (y = 0; y < HSV_HEIGHT; y += HSV_TILE) {
            unsigned saturation = y * 255 / (HSV_HEIGHT - HSV_TILE);
            for (x = 0; x < HSV_WIDTH; x += HSV_TILE) {
                unsigned hue = x * 1535 / (HSV_WIDTH - HSV_TILE);
                ui_background_color(dc, hsv_rgb(hue, saturation, 255));
                ui_clear_rect(dc, (int)x, (int)y,
                              (int)(x + HSV_TILE - 1), (int)(y + HSV_TILE - 1));
            }
            /* Bound continuous rendering work; manager gets scheduler time. */
            OSTimeDly(1);
            if ((y % 32) == 0) log_value("HSV rows completed", y + HSV_TILE);
        }
        elapsed = OSTimeGet() - start_ticks;
        log_value("rectangle rendering ticks (includes progress logging)", elapsed);
#endif
        log_value("RETURNED HSV map", 0);
    }
#else
    log_value("BEFORE panel background color", 0);
    ui_background_color(dc, 0x102030);
    log_value("BEFORE panel clear_rect", 0);
    ui_clear_rect(dc, 0, 0, 239, 199);
    log_value("RETURNED panel clear_rect", 0);
    ui_background_color(dc, keys ? 0x00ff00 : 0xff8000);
    /* An orange inset becomes green when an extended key arrives. */
    ui_clear_rect(dc, 16, 16, 223, 183);
#ifdef PROBE_FONT
    if (font_ready) {
        struct ui_rect rect = { 20, 24, 219, 55 };
        int text_status;
        ui_color(dc, 0xffffff);
        ui_text_mode(dc, 2);
        ui_font_size(dc, 16);
        log_value("BEFORE text X7 font test", 0);
        text_status = ui_text(dc, "X7 font test", &rect, 1, 0);
        log_value("text return (semantics unconfirmed)", (x7_u64)(x7_s64)text_status);
        rect.top = 64; rect.bottom = 95;
        log_value("BEFORE text START exits", 0);
        text_status = ui_text(dc, "START exits", &rect, 1, 0);
        log_value("text return (semantics unconfirmed)", (x7_u64)(x7_s64)text_status);
    }
#endif
#endif /* PROBE_HSV */
    log_value("BEFORE screen update", 0);
    {
        x7_u32 start_ticks = OSTimeGet();
        ui_screen_update();
        x7_u32 elapsed = OSTimeGet() - start_ticks;
        log_value("screen update ticks", elapsed);
    }
    log_value("RETURNED screen update", 0);
}
static void finish(unsigned why)
{
    reason = why; done = 1;
    ui_exit_loop();
}
static void timeout(void *arg)
{
    (void)arg;
    finish(2);
}
static void system_message(struct ui_app_message *msg)
{
    /* MSG_APP_QUIT, from calculator DWARF and its system dispatcher. */
    if (msg->type == 1001) finish(3);
}
static int window_message(struct ui_message *msg)
{
    if (messages++ < 64) {
        log_value("GUI msgid", (x7_u64)(x7_s64)msg->msgid);
        log_value("GUI handles (src:win)", ((unsigned)msg->hwinsrc << 16) | msg->hwin);
        log_value("GUI data word", msg->data.word);
    }
    /* Calculator's extended-key branch at 0x65804264 reads this pointer. */
    if (msg->msgid == 13 && msg->data.key) {
        last_key = msg->data.key->val;
        last_type = msg->data.key->type;
        ++keys;
        if (keys <= 128) {
            log_value("KEY val", last_key);
            log_value("KEY type", last_type);
            log_value("KEY ticks", OSTimeGet());
        }
#ifndef PROBE_HSV
        draw();
#endif
        if (last_key & 8) finish(1);
        if (keys >= 128) finish(4);
        return 0;
    }
    return ui_default_callback(msg);
}
int ui_probe_run(int (*record)(const char *, x7_u64), int (*before)(const char *))
{
    struct ui_app_message msg;
    void *gui = 0;
    int window = 0, timer = -1, registered = 0, status = -1, n;
    unsigned events = 0;
    log_value = record;
    if (!before("BEFORE dlopen gui.so")) goto out;
    gui = x7_dlopen("gui.so", 1);
    record("gui handle", (x7_word_t)gui);
    if (!gui) {
        const char *error = x7_dlerror();
        record("GUI loader error pointer", (x7_word_t)error);
        if (error) record(error, 0);
        goto out;
    }
    record("SKIP default font getter: v2 stopped there", 0);
#ifdef PROBE_HSV
    record("configured width (theme s480272)", HSV_WIDTH);
    record("configured height (theme s480272)", HSV_HEIGHT);
    record("HSV tile size", HSV_TILE);
    if (!before("BEFORE create fullscreen window")) goto out;
    window = ui_create_window(0, 0, HSV_WIDTH, HSV_HEIGHT, 2, window_message, 0);
#else
    if (!before("BEFORE create 240x200 window")) goto out;
    window = ui_create_window(0, 0, 240, 200, 2, window_message, 0);
#endif
    record("window handle", (x7_u64)(x7_s64)window);
    if (window <= 0) goto out;
    if (!before("BEFORE focus and get DC")) goto out;
    ui_set_focus((unsigned)window);
    dc = ui_dc_get((unsigned)window & 0xffff);
    record("DC pointer", (x7_word_t)dc);
    if (!dc) goto out;
#ifdef PROBE_FONT
    if (!before("BEFORE create ATTFV1.TTF size 16")) goto out;
    font_id = ui_create_font("/mnt/sdisk/ATTFV1.TTF", 16);
    record("font id", (x7_u64)(x7_s64)font_id);
    if (font_id < 0) goto out;
    if (!before("BEFORE select default fontface")) goto out;
    n = ui_default_fontface(font_id);
    record("fontface status", (x7_u64)(x7_s64)n);
    if (n < 0) goto out;
    font_ready = 1;
#endif
#ifdef PROBE_HSV
    record("ticks before initial draw", OSTimeGet());
#endif
    if (!before("BEFORE initial draw")) goto out;
    draw();
    if (!before("BEFORE register system dispatcher")) goto out;
    ui_register_dispatcher(system_message); registered = 1;
    if (!before("BEFORE 20000ms exit timer")) goto out;
    /* A repeating timer keeps its handle valid until explicit kill on exit. */
    timer = ui_set_timer(20000, timeout, 0);
    record("timer id", (x7_u64)(x7_s64)timer);
    if (timer < 0) goto out;
    if (!before("BEFORE message loop")) goto out;
    while (!done && events < 256) {
        n = ui_get_msg(&msg);
        if (n != 1) { record("get_msg terminal result", (x7_u64)(x7_s64)n); break; }
        ++events;
        if (events <= 32) record("APP message type", (x7_u64)(x7_s64)msg.type);
        ui_dispatch_msg(&msg);
    }
    record("exit reason (1=start 2=timer 3=quit 4=key limit)", reason);
    record("app messages dispatched", events);
    record("GUI messages", messages);
    record("key events", keys);
    status = 0;
out:
    /* Cleanup is unconditional once acquired, including after a log failure. */
    if (timer >= 0) { before("BEFORE kill timer"); ui_kill_timer(timer); }
    if (registered) { before("BEFORE unregister dispatcher"); ui_unregister_dispatcher(system_message); }
#ifdef PROBE_FONT
    font_ready = 0;
#endif
    dc = 0;
    if (window > 0) { before("BEFORE delete window"); ui_delete_window((unsigned)window); }
#ifdef PROBE_FONT
    if (font_id >= 0) { before("BEFORE destroy font"); ui_destroy_font(font_id); }
#endif
    if (gui) { before("BEFORE dlclose gui"); record("gui dlclose", (x7_u64)(x7_s64)x7_dlclose(gui)); }
    record("UI probe status", (x7_u64)(x7_s64)status);
    return status;
}
