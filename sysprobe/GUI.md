# X7 GUI, fonts and buttons

This is the current reference for the recovered GUI API. The interfaces below
have working examples on this console unless explicitly marked otherwise.
[UI-notes.md](UI-notes.md) retains the investigation history;
[FRAMEBUFFER.md](FRAMEBUFFER.md) describes the faster driver video path, and
[DEMOS.md](DEMOS.md) compares the rendering experiments.

## ABI and libraries

GUI traps use the vendor MIPS little-endian O32 convention:
`lui v1,group; ori v1,index; syscall`. The trap returns to the caller's `ra`;
the stubs deliberately have no `jr ra`. Group 3 belongs to GUI, and group 18
(`0x12`) to application services. These are separate from the two SYSCFG
arrays covered by the root [syscall reference](../syscalls.md).

Declarations and exact stubs are in [ui_abi.h](ui_abi.h) and
[ui_stubs.S](ui_stubs.S); [check_app.py](check_app.py) compares the 24 stubs
with named vendor application stubs. Caller/DWARF evidence principally comes
from `calculat.app`, with rectangle, bitmap and timer callers in `ebook.app`,
`anim_off.app`, `calibrat.app` and `manager.app`.

Pathless `x7_dlopen("gui.so",1)` works on the console. The supplied copy is
on `/mnt/sdisk`; obtaining a handle alone did not prove drawing worked.
The runtime also loads `libc_fs.so` with flag 2 and
`/mnt/diska/lib/applib.so` with flag 1. `applib.so` is absent from the supplied
binary set, so its timer and message-loop implementation has not been traced.

Calculator dependencies include applib, commonui, fusion, GUI, style and
apconfig. The default-font dependency experiment loaded candidates before
`applib_init`; the all-candidate order was applib/commonui/fusion/GUI/style/
apconfig. Simple drawing and direct-path fonts do not require those extra
widget/effect libraries in our probes.

## Application and drawing sequence

Use the existing [crt.c](crt.c) and [probe.c](probe.c) as the complete example.
The launcher calls `__start`; the CRT registers the process and starts the
worker. The worker loads runtime libraries and calls
`app_ui_init(argc,argv,NULL)` (the recovered `applib_init` wrapper).
For a GUI scene:

1. Load GUI, create a window, set focus and acquire its drawing context (DC).
2. Set the DC background before clearing; foreground color does not control
   `clear_rect`. Draw rectangles, text or bitmaps and call `ui_screen_update()`.
3. Register a system-message handler and bounded timers, then run the message
   loop below. Keep the callback, buffers and dependent libraries alive.
4. On exit, kill timers, unregister the handler, delete the window and close
   GUI. Finish with `app_ui_quit()`, library close and application exit.

```c
struct ui_app_message msg;
while (!done) {
    int result = ui_get_msg(&msg);
    if (result != 1) break;
    ui_dispatch_msg(&msg);
}
```

`ui_get_msg` invokes window and timer callbacks internally. In the tested
probes the outer loop often receives no dispatchable messages, and the call
returns 0 after a callback requests `ui_exit_loop()`. Do not assume it is a
nonblocking key poll. The system handler treats application message type 1001
as a quit request; this follows the demo implementation, without a complete
catalogue of system message types.

Installing a probe as `calculat.app` and starting it from the tools launcher
works. Launching it from the browser menu did not work. Early probes left the
menu unresponsive or rebooted; the corrected lifecycle/message loop exits
cleanly. Merely drawing from an arbitrary thread is not a tested replacement.

## Window and drawing calls

The names here are our C wrappers, with their API indices within group 3.
Argument order is significant: O32 arguments beyond the fourth go on the stack.

| Wrapper | Index | Arguments / tested behavior |
| --- | --- | --- |
| `ui_default_callback` | `0x40` | `message*`; delegate unhandled window messages |
| `ui_create_window` | `0x48` | `x,y,width,height,flags,callback,parent`; tested flags 2, parent 0; positive handle |
| `ui_set_focus` | `0x4a` | `window_handle` |
| `ui_delete_window` | `0x4b` | `window_handle` |
| `ui_dc_get` | `0x56` | `window_handle`; returns opaque DC |
| `ui_text_mode` | `0x59` | `DC,mode`; text mode 2 tested |
| `ui_color` | `0x5a` | `DC,color_word`; foreground/text color |
| `ui_background_color` | `0x5b` | `DC,color_word`; sets rectangle clear color |
| `ui_font_size` | `0x5e` | `DC,size`; size 16 tested |
| `ui_text` | `0x6b` | `DC,text,rect*,alignment,option`; 1/0 tested, meaning not fully recovered |
| `ui_draw_bitmap` | `0x73` | `DC,pixels,x,y,width,height,bytes_per_pixel`; tightly packed 2-byte pixels tested |
| `ui_clear_rect` | `0x77` | `DC,left,top,right,bottom`; inclusive edges in tested calls |
| `ui_screen_update` | `0x3c` | no arguments; commits GUI drawing |

The physical LCD query and full-screen tests agree on **480×272**. A window
at `(0,0,480,272)` covers the display. A DC supports software drawing but has
not been recovered as a writable framebuffer pointer. The fast path uses
`/dev/fb` instead. GPU.KO/LCD.KO/FB.KO/TP.KO exist; there is no recovered general
GPU drawing or hardware 3D API in these examples.

A text rectangle is four signed 16-bit fields, in order left/top/right/bottom
(8 bytes). The bitmap call takes a raw pointer, not `gui_bitmap_info_t`.
Calculator DWARF's separate bitmap-info struct is 12 bytes with u16 width,
height and bpp at offsets 0/2/4, and a byte pointer at +8. Do not substitute
that struct for the seven bitmap arguments. The static bitmap probe retains
its buffer throughout the scene; the complete asynchronous lifetime contract
for arbitrary callers remains unknown.

## Color words and native pixels

The helpers build GUI color words as `(r<<16)|(g<<8)|b`. GUI.SO's conversion
at `0x40c001d0`, mirrored in [hsv.c](hsv.c), produces:

```c
unsigned short pixel = ((color >> 19) & 31)
                     | (((color >> 10) & 63) << 5)
                     | (((color >> 3) & 31) << 11);
```

Thus the high color byte maps to the low five pixel bits, green to the middle
six, and the low color byte to the high five. Pixels are 16-bit little-endian,
row-major, with no extra row padding in the bitmap example. A full screen is
261,120 bytes. Copy this packing rather than assuming a Linux RGB565 layout;
it produces matching rectangle/bitmap colors and the working video format 1
image. Full channel-order semantics beyond the tested appearances have not
been exhaustively verified. Gradients can visibly quantize into color bands.

## Window messages and button events

| Structure | Size | Fields and offsets |
| --- | --- | --- |
| `ui_message` | 12 bytes | s32 `msgid` +0; u16 `hwin` +4; u16 `hwinsrc` +6; pointer/word union +8 |
| `ui_key` | 8 bytes | u32 logical button mask `val` +0; u32 event `type` +4 |
| `ui_app_message` | 1032 bytes | s32 `type` +0; 1024 content bytes +4; s32 sender PID +1028 |

Window message ID **13** carries a pointer to `ui_key` at +8. Only access it
for that message and when non-NULL. Other message unions can contain unrelated
or uninitialized values. Creation/deletion IDs 3/4 and focus-related `0x6e`
were observed; ID 1 may be paint-related but is not fully established.

| SDK button names | Masks |
| --- | --- |
| Volume + / − | `0x1` / `0x2` |
| Select / Start | `0x4` / `0x8` |
| Up / Down / Left / Right | `0x10` / `0x20` / `0x40` / `0x80` |
| L / R | `0x100` / `0x200` |
| A / B / X / Y | `0x400` / `0x800` / `0x1000` / `0x2000` |
| Home / Lock | `0x4000` / `0x8000` |
| Power short / long | `0x10000` / `0x20000` |

These come from calculator DWARF, not an exhaustive physical mapping test.
START `0x8` is console verified and exits the demos. Event types are DOWN
`0x20000000`, LONG `0x10000000`, HOLD `0x08000000`, SHORT_UP `0x04000000`,
LONG_UP `0x02000000`, HOLD_UP `0x01000000`; ALL is `0x3f000000`. Press and
short release match console logs. One event had both words zero. Inspect type
as well as mask when distinguishing presses, release and repeats. KEY.KO
contains scanning/timer code, but no direct `/dev/key` polling ABI is verified.

## Application services and timers

| Wrapper | Group-18 index | Arguments / behavior |
| --- | --- | --- |
| `ui_set_timer` | `0x0f` | `milliseconds,void callback(void*),context`; nonnegative timer ID tested |
| `ui_kill_timer` | `0x14` | `timer_id` |
| `ui_get_msg` | `0x38` | `ui_app_message*`; loop while result is 1 |
| `ui_dispatch_msg` | `0x3d` | `ui_app_message*` |
| `ui_register_dispatcher` | `0x3e` | system-message callback |
| `ui_unregister_dispatcher` | `0x3f` | same callback |
| `ui_exit_loop` | `0x40` | no arguments; exit the application message loop |
| `ui_font_file` | `0x7b` | no arguments; library-owned default-font basename |

Requests of 20/30 seconds exit normally, and 10 ms/1 ms frame timers animate.
Actual timer resolution, overdue-event handling, and OS ticks per wall-clock
second are not established. Startup frame submission bursts are measured;
"timer catch-up" is still a hypothesis. Bounded callbacks keep returning to
the message pump. See [watchdog findings](../libg1_probe/README.md#watchdog-findings)
for the separate manager/device watchdogs; the GUI demo does not disable them.

## Font API and dependency

| Wrapper | Group-3 index | Arguments |
| --- | --- | --- |
| `ui_create_font` | `0x2b` | `path,size`; returns a font ID |
| `ui_destroy_font` | `0x2c` | `font_id` |
| `ui_default_fontface` | `0x58` | `font_id` only, not a DC |

`/mnt/sdisk/ATTFV1.TTF` is an 11,003,096-byte TrueType font, with 12 SFNT
tables and WenQuanYi Zen Hei / Medium name records. CONFIG.BIN's DEFAULT_FONT
value is lowercase `attfv1.ttf` at file offset `0x6ae7`. FFT.KO contains
FreeType creation/selection code and recognizes TTF files.

The working direct-font sequence is create(path,16), select the ID, set
DC text mode 2, white foreground and size 16, then draw ASCII strings with
rectangle options 1/0. The verified run returned font ID `0x26cc`, face-select
result 0, and text results `0x88` / `0x89`. Those positive text returns are
uninterpreted; they are not documented zero-success statuses. Font destruction
and shutdown worked. Unicode/shaping, other sizes and alignment modes remain
untested. Font IDs and runtime addresses from logs are examples, not constants.

The default-font getter stalled without a prerequisite. Loading **apconfig.so
before applib_init** made it return `attfv1.ttf`; all-four dependencies also
worked. Loading commonui, fusion or style alone did not resolve it: style led
to a watchdog reboot, fusion hung, and commonui's eventual result was not
observed. Their logs stop at the getter, after load/init returned. This proves
a dependency, not which library implements the getter. Keep apconfig and the
runtime libraries loaded while using the returned string; treat it as borrowed
storage. Resolve the basename to the independently tested absolute path.

## Evidence files

The [direct font log](../logs/appprobe-font.txt) and
[apconfig dependency log](../logs/appprobe-default-apconfig.txt) are archived
under [logs/](../logs/), alongside the other candidate and window/key runs.
[HSV tiles](../logs/appprobe-hsv.txt) and
[HSV bitmap](../logs/appprobe-bitmap.txt) hold the full-screen rendering results.
The text logs plus the user's visual observations establish visibility and
clean menu return; a successful syscall alone does not. [DEMOS.md](DEMOS.md)
links each archived rendering generation.
