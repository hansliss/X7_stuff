# GUI and framebuffer investigation history

Current references: [GUI.md](GUI.md), [FRAMEBUFFER.md](FRAMEBUFFER.md), and
[DEMOS.md](DEMOS.md). This file preserves the order of discoveries and older
probe failures. Later console results supersede early uncertainty; version
numbers belong to individual binaries. See the current references when
implementing new code.

## Initial screen and button investigation

The calculator uses the same vendor `lui v1; ori v1; syscall` mechanism for
GUI, application messages, and drawing helpers. These are additional library
API groups, not additions to the two root SYSCFG tables (groups 1 and 2).
The evidence here comes from the named stubs, callers, and DWARF in
`binaries/calculat.app`; at that initial investigation stage the UI calls had not been console-probed.
The later sections and current references document successful testing.

| Function | API number | Vendor stub |
| --- | --- | --- |
| gui_wm_create_window | 0x30048 | 0x658084f0 |
| gui_wm_set_focus | 0x3004a | 0x658084fc |
| gui_wm_delete_window | 0x3004b | 0x65808508 |
| gui_dc_get | 0x30056 | 0x65808520 |
| gui_dc_set_color | 0x3005a | 0x65808544 |
| gui_dc_display_string_in_rect | 0x3006b | 0x65808574 |
| get_msg | 0x120038 | 0x6580705c |
| dispatch_msg | 0x12003d | 0x65807080 |
| register_sys_dispatcher | 0x12003e | 0x6580708c |
| exit_msg_loop | 0x120040 | 0x658070a4 |
| clear_key_msg_queue | 0x120042 | 0x658070b0 |

Other visible layers include group 13 (0x0d) style/resource drawing, group 19
(0x13) common UI widgets, and group 23 (0x17) fusion display/effects.
For example `fusion_display` at 0x65807200 is API 0x170003.
The calculator loads gui.so, applib.so, commonui.so, style.so, fusion.so,
and apconfig.so before using their respective interfaces. The applib library
is outside the supplied binary set; it exists on the console's diska.

At 0x65805b80 the calculator calls get_msg with an output-object pointer,
continues while its return is 1, and passes the same object to dispatch_msg.
Its GUI window callback is `_calculat_scene_callback` at 0x658041a0. This is
event-driven input rather than a direct polling call to read a button bitmap.
Button events are handled through that callback; callers must preserve the
app lifecycle and focus/message handling, not merely issue a GUI trap.

## Recovered logical button constants

Calculator DWARF (key enumeration at DIE 0x2b1e) provides bit masks:

| Button | Mask |
| --- | --- |
| Volume + / - | 0x1 / 0x2 |
| Select / Start | 0x4 / 0x8 |
| Up / Down / Left / Right | 0x10 / 0x20 / 0x40 / 0x80 |
| L / R | 0x100 / 0x200 |
| A / B / X / Y | 0x400 / 0x800 / 0x1000 / 0x2000 |
| Home / Lock | 0x4000 / 0x8000 |
| Power short / long | 0x10000 / 0x20000 |

These are logical SDK names and values, not a console-verified mapping of
every physical button on this particular hardware.

`key_event_t` is 8 bytes: a 32-bit `val` button mask at +0 and 32-bit `type`
at +4 (DWARF DIE 0x31e8). Type values are DOWN 0x20000000, LONG 0x10000000,
HOLD 0x08000000, SHORT_UP 0x04000000, LONG_UP 0x02000000, HOLD_UP 0x01000000;
ALL is 0x3f000000. The callback loads and combines the two words when matching
events (e.g. 0x65804264..0x65804278). The mask alone does not identify press,
release, or repeat behavior.

## Lower-level devices

The firmware set also includes FB.KO, LCD.KO, GPU.KO, KEY.KO, and TP.KO.
FB.KO strings include act_fb_ioctl and framebuffer buffer/allocation routines;
KEY.KO includes key scanning timer and key driver routines. This establishes
lower-level driver code exists, but not a ready-to-use framebuffer mapping,
pixel format, key device path, or ioctl ABI. At this stage no direct /dev/fb or /dev/key interface followed from names
alone. The later framebuffer investigation recovered and tested /dev/fb;
a direct key-device ABI remains unverified. The generic sys_open/sys_ioctl/sys_mmap
wrappers can support driver interfaces once their paths and commands are
recovered.

The application message/window layer is the clearest starting point for a
visible text-and-button probe. The GUI callback message is 12 bytes: 32-bit msgid at +0, 16-bit hwin
at +4, 16-bit hwinsrc at +6, and a 4-byte data union at +8 (DWARF
DIE 0x2ac6). The outer application message is 1032 bytes: signed type at +0,
1024 content bytes at +4, signed sender_pid at +1028. Window creation uses
seven O32 argument words: x, y, width, height, flags, callback, parent; calculator
uses flags=2 and parent=0. The rectangle passed to text drawing has four signed
16-bit edges, and text drawing takes DC, string, rectangle pointer, alignment,
and option (calculator uses 1,0). Font creation takes path,size; default fontface
setting takes just the font ID, not a DC. The probe's clear_rect takes
DC,left,top,right,bottom, as seen in ebook.app at 0x6781267c.

Display commit uses gui_screen_update (group 3, index 0x3c), as in calibrat.app.
Timer creation uses set_timer(milliseconds, callback, argument), group 18 index
0x0f; manager.app supplies a callback and NULL argument. kill_timer(id) is index
0x14. These prototypes are inferred from callers; the initial sysprobe-ui.app was
the console experiment. Later runs verified the drawing, messages and timers.
See README.md for installation and log interpretation.


## UI v3 console results

The supplied v3 log and observed black panel confirm window creation (handle
0x34), focus, DC acquisition, clear_rect, screen_update, extended-key callback
layout, a 20000ms timer and cleanup. There were 31 key events. Press and short
release types match DWARF. One event contained zero in both key words; preserve
raw diagnostics rather than treating every callback as a physical press.

get_msg delivered GUI callbacks and the timer internally; the outer loop
received no dispatchable messages and get_msg returned 0 after the timer's
exit_msg_loop request. Cleanup reached DONE with no probe failures, and the
user observed normal timed exit. This message loop avoided the earlier reboot.

The ebook's gui_dc_set_background_color stub at 0x67818e34 is group 3/index
0x5b. Its caller at 0x67811d84 supplies DC and color. v4 sets this background
state before clear_rect, replacing the foreground-color mistake in v3.
Font APIs remain deliberately uncalled after the v2 font-getter stall.


## UI v4 console results

The user confirmed the corrected colored panel works and pressing START
terminates the application. This verifies background-color setting with
clear_rect and the SDK START mask 0x8 on this console. The preceding v3 run
verified timer-driven exit and extended button events. Font discovery and text
rendering remain unresolved; no font call is made in v4.


## Direct font experiment

The complete sdisk copy supplies ATTFV1.TTF, a valid SFNT TrueType font of
11,003,096 bytes, with 12 table records and WenQuanYi Zen Hei / Medium name
records. CONFIG.BIN contains DEFAULT_FONT and the lowercase value attfv1.ttf
(value offset 0x6ae7). FFT.KO includes FreeType creation/selection routines and
recognizes TTF suffixes. The matching GUI/font binaries provide no reason to
change the already recovered syscall stubs.

sysprobe-font.app bypasses sys_get_default_font_file, passing the absolute
font path to gui_create_font(path,16), selecting the returned font ID, and
rendering two ASCII strings in separate rectangles. This sequence is now console
verified by the font v1 run below. It distinguishes failure to discover a default font from failures
in font creation, selection or drawing. Text mode 2 and rectangle options 1/0
are copied from calculator call sites; their full semantics remain uncertain.


## Font v1 console results

The user observed the expected text and behavior; `appprobe-font.txt` confirms:

- Direct-path font creation at size 16 returned ID 0x26cc.
- Default fontface selection returned 0.
- Foreground white, text mode 2, font size 16 and rectangular text drawing
  with options 1/0 produced the expected visible ASCII strings.
- Text calls returned 0x88 and 0x89, reproducibly on the initial and key redraw.
  These positive values are not interpreted as conventional zero-success
  status; their meaning (possibly text extent) remains unverified.
- START mask 0x8 with type DOWN 0x20000000 triggered exit reason 1.
- Font destruction and library/app cleanup completed; DONE and zero failures
  were logged, and the user observed normal application behavior.

Thus font loading, selection, ASCII drawing and cleanup now have a working
console-tested example. At this stage the default-font getter was an independent open question;
the subsequent dependency experiment resolved its apconfig prerequisite. Earlier UI logs are archived in
`logs/appprobe-ui_*.txt`, while the font run is now archived in
`logs/appprobe-font.txt` (paths relative to the workspace root).


## Default-font dependency console results

The five supplied logs/appprobe-default-*.txt logs establish that adding apconfig.so
before applib_init is sufficient in this probe to make sys_get_default_font_file
return. Both apconfig-only and all-four runs returned pointer 0x4741708c and
string `attfv1.ttf`, completed all library/app cleanup, and logged zero failures
and DONE. The all-four run lasted 53 ticks; apconfig-only lasted 31 ticks.

commonui-only, fusion-only and style-only all loaded their candidate and GUI,
returned from applib_init, then stopped at BEFORE default font getter. Thus the
failure point is the getter, not dlopen or init. The user observed a watchdog
reboot for style and a prolonged hang for fusion; commonui's eventual outcome
was not observed. These extra libraries individually did not resolve the getter.

This is a demonstrated initialization dependency, not proof that apconfig
implements the group-18 getter itself. Retain apconfig through application
shutdown when using it. The returned string is a basename, not an absolute
path; the independently verified font path is /mnt/sdisk/ATTFV1.TTF. The string
storage lifetime should conservatively be treated as library-owned while the
libraries remain loaded. No further failed-candidate runs are needed to identify
the prerequisite in this firmware.


## Full-screen HSV experiment

CONFIG.BIN includes the s480272 theme, consistent with the observed 240-pixel
window covering half the screen width. sysprobe-hsv.app requests 480×272 at
(0,0) with the same flags/callback/focus as the tested window. These are
configured dimensions, not runtime-measured framebuffer dimensions.

The map is rendered via background-color/clear_rect in 4×4 tiles with
hue across X, saturation across Y, and full value. It requires no new GUI trap
or bitmap/framebuffer ABI. Row yields limit continuous CPU occupation; one
screen_update commits the complete map. START/timer exits and cleanup remain
shared with the tested probe. The following result verifies full-screen behavior.


## HSV v1 console result

The user confirmed the map fills the screen and behaves as described.
`appprobe-hsv.txt` confirms the 480×272 window, all tile rows and screen update
completed, timer exit reason 2, successful cleanup, DONE and zero failures.
The worker's total lifetime was 2541 ticks (0x12452 to 0x12e3f), including the
20-second display interval. No key was pressed in this recorded run.

Startup was visibly slow. The probe issues 16,320 color/rectangle GUI traps,
yields 68 ticks while drawing, and synchronizes each diagnostic record to SD.
Those are plausible contributors, but the log lacks per-stage tick timestamps
and cannot attribute the delay precisely. A bitmap blit or framebuffer-backed
canvas remains a possible optimization once its ABI is recovered.


## Bitmap HSV experiment

anim_off.app's _proc_timer_paint at 0x60c005b4 passes seven words to
GUI group 3/index 0x73 (gui_dc_draw_bitmap_ext): DC, raw pixels, x, y, width,
height and bytesperpixel=2. GUI.SO's seven-word wrapper at 0x40c0d848 selects
the DC, then forwards the remaining arguments to 0x40c0be54. The separate
12-byte gui_bitmap_info_t in calculator DWARF has u16 width/height/bpp at
0/2/4 and a const byte pointer at +8; this struct is not the blit's argument.

GUI.SO 0x40c001d0 converts its color word to native 5:6:5: extract bits 19..23
to pixel 0..4, bits 10..15 to pixel 5..10, bits 3..7 to pixel 11..15.
sysprobe-bitmap.app uses that conversion on the same HSV color words used by
the verified rectangles. Its 480×272 static pixel buffer is 261,120 bytes and
survives through cleanup. One blit replaces 16,320 rectangle/color traps.
The new test includes generation/blit/screen-update timing; rectangle v2 adds
comparable stage timestamps. The following result verifies the bitmap ABI and appearance.


## Bitmap HSV v1 console result

The user confirmed correct full-screen bitmap rendering. appprobe-bitmap.txt
records a 261,120-byte buffer, generation in 9 ticks (including 8 deliberate
one-tick yields), one gui_dc_draw_bitmap_ext call returning 0 in 1 tick, and
screen_update within one tick (recorded delta 0; not literally zero duration).
Runtime/applib setup took 12 ticks from initial worker timestamp, and drawing
began after another 15 ticks of GUI/window setup and diagnostic logging.

START mask 0x8/DOWN triggered exit reason 1. Window/library cleanup completed,
with DONE and zero failures. The seven-word bitmap ABI, tightly packed
480×272 two-byte pixels, native 5:6:5 conversion, and retained static buffer now
have a working console-tested example. Framebuffer mapping is not required.
The prior rectangle log is v1 without rendering timing, so an exact speedup
ratio cannot be calculated from these two runs.


## Dodecahedron v1 console result and v2 experiment

The user confirmed rotation and flat point lighting worked. The log records
276 frames, 249 render ticks, 1064 bitmap-blit ticks and 17 update ticks;
first frame render rounded to zero ticks and blit took one. Timer exit reason
2, successful cleanup, DONE and zero failures confirm normal completion.
The nominal 30-second interval suggests roughly 9 FPS; elapsed frame-loop
wall time was not separately timestamped. Bitmap copying dominates measured
cost. The background's subtle gradient appeared as three 16-bit color bands.

v2 clears the full 480×272 window to one solid color, then renders/blits only
240×224 at screen (120,24). This preserves projection scale and center while
reducing bitmap bytes from 261120 to 107520. Every frame redraws that entire
box, clearing old silhouettes. A 1200-frame host test checks clear borders so
no part of the rotating object is clipped. The frame timer decreases from
50ms to 10ms; a one-tick yield remains. START/30-second timeout/cleanup remain.
The revised animation still advances by frame count, so its rotation speed
will change with actual frame rate. The following result records v2 performance.


## Dodecahedron v2 console result

The user reported much faster, smoother animation. The v2 log records 617
frames versus v1's 276, both ending via the nominal 30-second timer. This is
2.24 times as many frames (roughly 21 versus 9 FPS using the requested timer
interval; actual frame-loop wall time was not separately recorded).

Totals: render 57 ticks, blit 1061 ticks, update 594 ticks. Per-frame blit cost
fell from about 3.86 ticks to 1.72 ticks. Measured update cost increased from
about 0.06 to 0.96 ticks/frame, so timing should not assume display update is
free or that rendering/copy/update stages behave independently. Reduced buffer
size and a shorter frame timer were changed together; their individual effects
were not isolated. The framebuffer-sized window and smaller object update box
worked correctly, and timeout cleanup completed with zero failures.


## Emulator display-path investigation

emulator.app opens /dev/fb in open_fb_drv at 0x688097e0, switches game/UI
framebuffer state via emu_switchfb_togame/toui (0x68809b1c/0x68809b60), and
uses driver video-mode/blending ioctls rather than GUI bitmap drawing for the
game display. Its GUI calls serve menus and overlays.

LIBEMU.SO output initialization at 0x50801360 opens /dev/fb and allocates
0x385800 bytes through group-1 slot 114 (allocation flags 10). It divides this
pool into five frame slots with stride 0x96400, plus a sixth scratch slot.
A slot is 640*480*2 bytes plus 1024 bytes padding. The current slot index at
0x5080e1e4 advances modulo five in code at 0x50800d70. This shows buffered
presentation rather than a single GUI canvas buffer.

The path at 0x50800cd8 obtains source width/height and mode through group-8
API 2 (command 13), selects a ring slot, passes its pointer through group-1
slot 116, and supplies the resulting address to /dev/fb ioctl 0x4667 at
0x50800dec. Width/height/mode are applied separately through ioctl 0x5012 when
they change. Command 14 at 0x50800c98 provides the current raw slot pointer
through the group-8 interface. The software copy path at 0x50800f78 can also
copy/convert source rows into a ring slot, so this is not proof of zero-copy
from the emulator core to the LCD.

FB.KO dispatches 0x4667 to 0xc2c06f5c and contains DMA-transfer/video output
code, with explicit source/output size parameters. Exact scaling, synchronization,
cache management and buffer ownership remain to be recovered. This establishes
a distinct driver/video path that avoids gui_dc_draw_bitmap_ext; it does not
establish a measured emulator FPS or that submission is a pure page flip.
Our dodecahedron also retains a one-tick yield and timer pacing absent from
this traced presentation sequence. This motivated the direct-driver experiments below, which subsequently
verified setup, drawing and UI restoration.

### Direct framebuffer demo (v3 console verified)

`fb_video.c` follows LIBEMU.SO's video buffer path, not Linux fbdev:

- `/dev/fb`, open flags 2; ioctl `0x4662` copies an 11-word LCD parameter block.
  The probe requires its first two words to be 480 and 272.
- Group-1 slots 114/115 allocate/free a pool; allocation flag `0x0a` matches
  LIBEMU. Five buffers rotate, with 1024 bytes of padding after each frame.
  Slot 116 converts each CPU pointer into the address submitted to the driver.
- `0x5012` copies 44 bytes of video parameters: width, height, stride in pixels,
  four zero words, format 1, then three zero words. LIBEMU's block is BSS and
  initializes exactly width/height/stride/format and word 8. Format 1 is now visually verified with the documented pixel packing;
  broader format semantics remain unresolved.
- `0x4660` starts video. `0x4667` copies a 12-byte plane-address block;
  for this packed format only the first address is supplied, with the other
  two words initialized to zero. The driver requires video to be started.
- `0x4661` stops video and restores the UI path. Pool freeing is deferred until
  stop succeeds plus three OS ticks. A failed stop retains the pool to avoid
  leaving the display engine reading freed memory.

Evidence: LIBEMU.SO setup at 0x50801e00, frame submission at 0x50800cd8;
FB.KO parameter copy at 0xc2c04a5c, plane copy at 0xc2c04a9c, start at
0xc2c070fc, stop at 0xc2c061c0. Video submission includes driver synchronization,
so it may still spend time waiting for scanout. Five buffers match the vendor
ring; this is not proof of a documented completion/fence contract.

The demo keeps the proven GUI window/message/timer lifecycle solely for input
and shutdown. It does not change LCD geometry or alter aspect mode via 0x5013.
Version 3 adds the emulator's full-screen blending region and display activation
sequence, described below. Video parameter settings are not saved
and restored: the intended entry point is the tools menu with video inactive,
and shutdown stops video before deleting the GUI window.

First console run (v1): LCD query returned 480×272 with 16-bit depth.
Pool allocation returned NULL, so no video setup or submission was attempted;
normal cleanup returned to the menu. The allocation flag was mistakenly
transcribed as hexadecimal 0x10. LIBEMU instruction at 0x508013cc sets a1 to
decimal 10 (0x0a). Version 2 corrects this; allocation size and ring are unchanged.

Second console run (v2): allocation, mode setup, start, submissions, stop and
free all succeeded. It rendered 994 frames over 3185 OS ticks (30-second
timeout), but the screen remained dark. Rendering consumed 915 ticks; summed
submission time was zero at OS tick resolution, which does not establish that
scanout was running. Thus the invisible run cannot yet establish visible FPS.

Version 3 mirrors emulator.app's additional display setup: `0x4675` with
nine words `{0,0,0,0,0,0,480,272,0}`, then `0x4676` with `{0,-1}`.
These follow `video_create_whole_fb_blending` at 0x688098c0. After video starts,
`0x4666(NULL)` follows `video_start_video` at 0x68809a0c. In FB.KO this last
command enters the display synchronization/update path; previously it was
omitted. Cleanup stops video, calls `0x4677({0,-1})` and `0x4679(NULL)`
(the emulator's `video_destroy_whole_fb_blending` at 0x688099a4), then frees
the stopped video pool. Each operation is separately logged. This tests the
hypothesis that the opaque GUI layer / unactivated display path hid the video;
which of these steps was essential is still unknown.

Third console run (v3): the user reports smooth, fast animation with correct
appearance. All blending setup, display activation, video stop, blending
cleanup, pool free and close calls returned zero; no assertion failures.
The 30-second timer ended after 990 submitted frames, versus 617 for bitmap
v2 (about 60% more frames per nominal timer run). Render total 915 OS ticks;
submission total zero at tick resolution. The elapsed counter was 3581 ticks,
including startup diagnostics between the first frame and timer creation and
post-loop diagnostic writes before its capture. It is not a clean 30-second
measurement or a hardware scanout counter. Nominal timer-based throughput is
about 33 frames/s versus 21 for bitmap v2; exact visible FPS remains unmeasured.
This run verifies the packed format and pool address path visually; the
blending region plus activation fixes visibility, but their individual
necessity has not been tested. Existing timer and deliberate one-tick yield
still limit throughput.

### Third demo: faster framebuffer variant

`PROBE_FAST` / `sysprobe-dodeca-fast.app` preserves the verified v3 display
setup and cleanup. It removes the per-frame OSTimeDly(1), requests a 1 ms
frame timer, and batches at most four frames or one elapsed tick per callback.
This bounds the time spent outside message dispatch; no new polling/input ABI
is introduced. Firmware may quantize the timer, and intermediate submissions
may be superseded before display scanout. The existing five-slot ring remains
because the driver has no recovered explicit fence API.

Focal length is 525 (175% of 300); geometry, camera position, look_at and point
light remain the same. All 1200 tested rotation frames fit the 480×272 screen.
Triangle filling walks fixed-point edges to produce contiguous scanlines,
using aligned 32-bit stores to fill pixel pairs. Background clearing also
uses paired stores. The span pointer uses GCC may_alias to support shared
16/32-bit writes. Only this variant selects that rasterizer.

New loop-only frame/tick counters exclude startup diagnostic writes; timing
accumulators reset immediately before entering message processing. No card
writes are added to the rendering loop. Host sanitizer rendering and eight
mocked lifecycle scenarios passed; cross builds and vendor ABI checks passed.
The following fast-demo console results establish its behavior. Earlier demos retain their original
pacing and rendering paths.

Fast v1 console run: START exit after 1471 measured loop frames in 1180 ticks;
1472 total including the startup frame. Total measured render cost 508 ticks,
submission cost zero at tick resolution. All cleanup calls succeeded. User
reports extremely fast initial rotation, then slower but still fast animation;
the preceding framebuffer version had a less pronounced initial burst.
The mean submitted rate is 1.247 frames per OS tick. This is not a panel FPS
measurement, and no per-interval counters were present to establish whether
throughput changed or only visual impressions did.

Fast v2 decouples rotation from frame submissions: phase is elapsed ticks
divided by three, with an epoch set immediately before entering the message
loop. This approximates the prior framebuffer demo's steady angular speed,
without imposing a frame-rate cap. It also stores submission counts in 64
100-tick buckets and logs them after stopping video; the last bucket can be
partial. No new SD writes are added during rendering. The raw bucket data
can test a startup-throughput burst. Timer catch-up remains a hypothesis;
applib.so's implementation is absent from the provided binaries. Cross build,
vendor ABI checks and eight lifecycle mock scenarios pass for v2.

Fast v2 console run: constant angular speed, but a wide moving noise band
appeared near the center for the first few seconds. The first three 100-tick
buckets contain 348, 396 and 380 submissions; bucket 3 contains 126 and the
following full buckets contain exactly 100 each. Total measured frames 3774,
loop ticks 3281, render ticks 1307; timeout exit and cleanup succeeded. Thus
the startup submission burst is real. Its exact scheduling cause is unknown.

Fast v3 replaces blind ring reuse with an address guard, retaining the same
pacing and time-based rotation. FB.KO ioctl 0x5011 (0xc2c06908) copies 12 bytes
from device+0xf0: the current video address triplet. At 0xc2c04bb0 the driver
copies pending addresses at +0xe4 to that triplet during display processing.
Before rendering, the fast demo queries the triplet and selects a pool slot
whose address differs from every reported plane and both recent successful
submissions. This protects a delayed current frame and the pending transition
between the snapshot and the next submit. Query failure exits; unavailable
buffers skip that callback frame. Counts of rejected candidates, unavailable
buffers and failed queries are written at shutdown.

This is a software-address snapshot, not a proven hardware fence. Avoiding
reported current/recent buffers tests the hypothesis that startup burst reuse
caused the noise band; it does not establish tear-free scanout. No artificial
startup delay or per-frame sleep was added. The host mock holds the current
address fixed while submissions advance, asserts that it and the last two
submissions are never reused, and tests query failure cleanup. Nine fast
lifecycle scenarios and both cross-build/ABI checks pass. The following console result verifies the noise fix.

Fast v3 console verified: user reports perfect behavior, with the startup
noise band resolved. Log: 3437 measured-loop frames, 10450 elapsed OS ticks,
3683 rejected protected-slot candidates, zero unavailable-buffer callbacks,
zero address-query failures, timeout exit and zero assertion failures. Video
stop, blending disable/destroy and pool free all returned zero. The current/
recent-address guard fixes the observed corruption in this test, supporting
the buffer-reuse explanation. The query is still not a recovered hardware
completion fence. The elapsed count differs substantially from earlier runs;
do not infer visible FPS or a fixed seconds-per-tick conversion from these
runs. Raw timing data and successful visual result are retained separately.
