/* Experimental UI ABI, recovered from vendor app stubs/callers and DWARF. */
#ifndef X7_UI_ABI_H
#define X7_UI_ABI_H
#include "app_abi.h"
struct ui_key { x7_u32 val, type; };
struct ui_message {
    x7_s32 msgid;
    x7_u16 hwin, hwinsrc;
    union { struct ui_key *key; x7_u32 word; } data;
};
struct ui_app_message { x7_s32 type; x7_u8 content[1024]; x7_s32 sender_pid; };
struct ui_rect { signed short left, top, right, bottom; };
_Static_assert(sizeof(struct ui_rect) == 8, "rectangle ABI");
_Static_assert(sizeof(struct ui_key) == 8, "key event ABI");
_Static_assert(sizeof(struct ui_message) == 12, "GUI message ABI");
_Static_assert(sizeof(struct ui_app_message) == 1032, "app message ABI");
int ui_create_window(int, int, int, int, unsigned, int (*)(struct ui_message *), unsigned);
void ui_set_focus(unsigned);
void ui_delete_window(unsigned);
void *ui_dc_get(unsigned);
int ui_default_callback(struct ui_message *);
int ui_create_font(const char *, int);
void ui_destroy_font(int);
int ui_default_fontface(int);
void ui_text_mode(void *, int);
void ui_color(void *, unsigned);
void ui_background_color(void *, unsigned);
void ui_font_size(void *, int);
void ui_clear_rect(void *, int, int, int, int);
int ui_text(void *, const char *, const struct ui_rect *, int, int);
int ui_draw_bitmap(void *, const void *, int, int, int, int, int);
void ui_screen_update(void);
const char *ui_font_file(void);
int ui_get_msg(struct ui_app_message *);
void ui_dispatch_msg(struct ui_app_message *);
void ui_register_dispatcher(void (*)(struct ui_app_message *));
void ui_unregister_dispatcher(void (*)(struct ui_app_message *));
void ui_exit_loop(void);
int ui_set_timer(unsigned, void (*)(void *), void *);
void ui_kill_timer(int);
int ui_probe_run(int (*record)(const char *, x7_u64), int (*before)(const char *));
#endif
