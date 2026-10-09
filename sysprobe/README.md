# X7 application probes and demos

The current API references are [GUI.md](GUI.md) for windows, text, fonts,
buttons and messages, and [FRAMEBUFFER.md](FRAMEBUFFER.md) for video ioctls,
buffers and shutdown. [DEMOS.md](DEMOS.md) collects build/run instructions,
all rendering results and timing interpretation. [UI-notes.md](UI-notes.md)
retains the experimental history. Root [syscalls.md](../syscalls.md) covers
the filesystem/kernel dispatch tables; [libg1 notes](../libg1_probe/README.md)
cover the replacement library and watchdog investigation.

The latest working rendering demo is **sysprobe-dodeca-fast.app, fast v3**:
175% projected size, elapsed-time rotation, optimized span filling, no explicit
per-frame sleep, and guarded framebuffer reuse. It is console verified with
no startup noise and clean menu return. The GUI bitmap and base framebuffer
versions remain comparison binaries. All source-package binaries can be
rebuilt with `sysprobe/build.sh` or their corresponding CMake targets.

The sections below include earlier probe generations. Version numbers are
local to each binary, and historical failures do not describe the latest one.

## GUI bitmap dodecahedron demo

**v1 console result:** geometry, lighting and rotation worked, with a banded
background and choppy animation. The log records 276 frames during the nominal
30-second interval, 249 total rendering ticks, 1064 blit ticks, 17 update ticks,
and successful timeout cleanup with zero failures. Bitmap copying was the
largest measured cost. The subtle background gradient quantized into bands.

**Current v2** uses a solid full-screen background and updates only a centered
240×224 rectangle at (120,24), retaining the object's size and screen position.
Each frame copies 107,520 bytes instead of 261,120 (about 59% fewer), and the
frame timer interval is 10ms instead of 50ms. One-tick scheduler yields remain.
A 1200-frame host test confirms the entire rotating object stays within the
update rectangle with clear borders. The v2 console run confirms much smoother animation: 617 frames versus
276 in v1's nominal 30-second run (about 2.24× as many). Per-frame blit time
fell from about 3.86 to 1.72 ticks. Timeout and cleanup completed with zero
failures. Approximate frame rates from the requested interval are 21 versus
9 FPS; the log does not separately timestamp actual frame-loop wall time.

**`sysprobe-dodeca.app`** is a 480×272 animated software-rendering demo.
Install it as `calculat.app`. It draws a rotating regular dodecahedron with
12 flat-shaded pentagonal faces, perspective projection and a point light.
**START exits**; automatic exit occurs after **30 seconds** or **1200 frames**.
The 10ms timer requests frames frequently; actual frame rate depends on
rendering, bitmap copying and the retained scheduler yield. The first frame is drawn before entering the message loop.

Return `/mnt/card/appprobe-dodeca.txt` and report animation speed, appearance
and whether exit restores the menu. The log records first-frame timings, frame
count and accumulated CPU-render/blit/display-update ticks. Each frame yields
one tick. Routine frames do not write diagnostics to SD; input and setup/exit
records do. Startup failures use `/mnt/card/appprobe-dodeca-start-error.txt`.

The renderer uses integer math with 1024 units per world unit, a 256-entry sine
lookup, outward-wound faces, back-face removal, depth ordering, pentagon-to-
triangle rasterization, and face outlines. Lighting is evaluated once per
face: ambient plus Lambert diffuse from a positional light, attenuated by
squared distance. No GPU/3D API, float runtime or libc is required. Pixels use
the console-tested two-byte bitmap packing and one centered 240×224 blit per frame.

`dodeca_default_scene` in dodeca_render.c configures camera position **(3,2,6)**,
look-at **(0,0,0)**, up **(0,1,0)**, light position **(-2,3,2)** and focal length
**300 pixels** (about 49 degrees vertical field of view). The object rotates
about three axes; the camera and point light remain fixed in world space.
A small light marker is drawn only if its projected position is within the
screen. Camera input is bounded, and degenerate look-at/up vectors are rejected.
This renderer assumes the object stays in front of the near plane; it rejects
frames with vertices too close rather than implementing polygon near clipping.

Builds and exact syscall/ELF checks pass. Host checks verify 20 vertices,
12 pentagons, 30 equal-length shared edges, 1200 changing frames under Address
and Undefined Behavior Sanitizers, and animation/lifecycle failure paths.
`dodeca-preview.png` is a host-rendered preview, not a console screenshot.
To run the checks from sysprobe:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/dodeca_render.c dodeca_render.c hsv.c -o /tmp/x7-dodeca-render
ASAN_OPTIONS=detect_leaks=0 /tmp/x7-dodeca-render
cc -std=c11 -O2 -Wall -Wextra -Werror '-D_Static_assert(x,y)=' \
  -Wno-pointer-to-int-cast tests/dodeca_flow.c dodeca_demo.c dodeca_render.c hsv.c \
  -o /tmp/x7-dodeca-flow
for scenario in 0 1 2 3 4 5 6 7; do /tmp/x7-dodeca-flow "$scenario" || break; done
```

Leak detection is disabled because the local sandbox's tracing prevents
LeakSanitizer from running; the renderer allocates no heap memory. Address and
undefined-behavior checks remain active. Timer-driven rendering and clean shutdown are console verified; see
[DEMOS.md](DEMOS.md) for the bitmap and subsequent framebuffer results.

# Bitmap HSV probe

**Console-verified:** the user confirmed correct full-screen bitmap rendering.
The log measures generation at 9 ticks (including 8 deliberate yield ticks),
the single blit at 1 tick with return 0, and display update at less than one
measured tick (delta 0). START exit and cleanup completed with zero failures.
The previous rectangle log lacks stage timings, so no exact speedup ratio is
claimed. This establishes a working pixel-buffer canvas via the GUI bitmap API.

**`sysprobe-bitmap.app`** is the new test. Install it as `calculat.app`.
It generates the 480×272 HSV map into a persistent two-byte-per-pixel buffer
and draws it with **one gui_dc_draw_bitmap_ext call**. The color gradient is
per-pixel rather than 4×4 tiles. START exits; the 20-second timer starts after
drawing, as before. Return `/mnt/card/appprobe-bitmap.txt` and report whether
the appearance matches the previous map, startup feels faster, and exit works.

The log records ticks after runtime/applib setup, ticks before drawing,
buffer generation duration, blit return/duration and screen-update duration.
Generation includes eight one-tick yields but no per-row SD logging. Timings
around blit and update exclude logging their results. The 261,120-byte buffer
is static BSS and retained through window deletion; no dynamic allocation or
direct framebuffer mapping is used. Startup errors use
`/mnt/card/appprobe-bitmap-start-error.txt`.

For a comparison, optionally run the rebuilt `sysprobe-hsv.app` (rectangle
v2) too. It logs rectangle-rendering ticks in `/mnt/card/appprobe-hsv.txt`.
That interval includes its row yields and progress logging, so it measures
this diagnostic's actual drawing cost rather than isolated syscall overhead.

The new stub is group 3/index 0x73, byte-checked against anim_off.app at
0x60c02eac. Its caller at 0x60c005b4 supplies DC, pixel pointer, x, y, width,
height, bytes per pixel; the final argument is 2. GUI.SO's wrapper at
0x40c0d848 agrees with seven argument words. Packing follows GUI.SO's routine
at 0x40c001d0: low input color byte becomes the high five pixel bits, the
middle byte becomes six middle bits, and the high byte becomes five low bits.
This deliberately matches the existing rectangle API's color conversion;
raw bitmap format and channel appearance are now console-verified for this probe.
Blit return values are logged without assuming a zero-success convention.

Both builds and ELF/stub checks pass. Host tests verify all generated pixels,
one blit, no tile calls/repeated rendering on START, buffer dimensions and
cleanup paths. They do not emulate the GUI renderer. To run from sysprobe:

```sh
cc -std=c11 -Wall -Wextra -Werror -DPROBE_HSV=1 -DPROBE_BITMAP=1 \
  '-D_Static_assert(x,y)=' -Wno-pointer-to-int-cast \
  tests/ui_flow.c ui_probe.c hsv.c -o /tmp/x7-bitmap-flow
for scenario in 0 1 2 3 4 5; do /tmp/x7-bitmap-flow "$scenario" || break; done
```

# Full-screen HSV probe

**Console-verified:** the user confirmed the HSV map covers the full screen
and behaves as described. The supplied log records timer exit, successful
cleanup and zero failures. Startup was noticeably slow; this diagnostic uses
16,320 GUI traps, 68 one-tick drawing yields and synchronous SD logging.
Per-stage timings were not recorded, so their individual costs are unknown.

**`sysprobe-hsv.app`** creates a window at (0,0) sized **480×272** and fills it
with an HSV map. Install it as `calculat.app`. Hue runs left to right through
red/yellow/green/cyan/blue/magenta/red. Saturation increases top to bottom:
the top is white, the bottom has vivid colors. Value stays at 255 throughout.
Press **START** to exit, or wait for the **20-second timer after drawing**.

Return `/mnt/card/appprobe-hsv.txt` and report whether the map covers the whole
screen, whether its colors look correct, and whether exit restores the menu.
Startup failures use `/mnt/card/appprobe-hsv-start-error.txt`. The map is drawn
once; button events are logged without repainting it. No text overlays or font
calls are made, so the entire canvas remains available for the color map.

480×272 is selected from the configuration's s480272 theme and the observed
half-width 240-pixel probe, not from an unverified screen-query API. The log
labels these as configured dimensions. The map uses 4×4 rectangles: 120 columns
by 68 rows, 8,160 rectangles total, with screen_update after drawing completes.
Each completed row yields one tick, and progress markers are synced every eight
rows. This uses verified GUI calls rather than an assumed framebuffer layout.
No display-mode or global dimension setter is called.

Both CMake and build.sh produce the new binary. Color conversion lives in
hsv.c/h; GUI lifecycle remains shared with the tested UI probe. Host tests
verify six hue anchors, hue wrap, conversion against an independent floating
point reference, complete pixel coverage without gaps/overlaps, no repeated
map rendering on START, and existing cleanup/error paths. The full-screen
window and map are now console-verified.

From sysprobe, run the HSV host tests with:

```sh
cc -std=c11 -Wall -Wextra -Werror tests/hsv_color.c hsv.c -lm -o /tmp/x7-hsv-color
/tmp/x7-hsv-color
cc -std=c11 -Wall -Wextra -Werror -DPROBE_HSV=1 \
  '-D_Static_assert(x,y)=' -Wno-pointer-to-int-cast \
  tests/ui_flow.c ui_probe.c hsv.c -o /tmp/x7-hsv-flow
for scenario in 0 1 2 3 4 5; do /tmp/x7-hsv-flow "$scenario" || break; done
```

# Default-font dependency probes

**Console result: apconfig.so is sufficient to enable default-font discovery.**
The apconfig-only and all-four logs both returned `attfv1.ttf`, completed cleanup
and recorded zero failures. commonui-only, fusion-only and style-only loaded
successfully and completed applib_init, but stopped inside the default-font
getter. The user observed a style watchdog reboot and a fusion hang; commonui's
eventual outcome is unknown. There is no need to repeat those failed variants.

The working sequence loads apconfig.so before applib_init and retains it through
applib_quit. The getter returns a basename; the verified direct font path is
`/mnt/sdisk/ATTFV1.TTF`. Its returned string is conservatively treated as
library-owned. These results establish an initialization prerequisite without
claiming which library implements the getter internally.

Five new `.app` files test the default-font getter with additional libraries.
Install each as `calculat.app` individually; every run writes its own log.
There is no visible UI or timed wait: successful calls are logged and the app
exits immediately through the tested lifecycle. The getter previously stalled
and the console rebooted; a candidate that fails to fix that may repeat this.
Its last synced BEFORE marker is the diagnostic result.

| Binary | Added library | SD log |
| --- | --- | --- |
| sysprobe-default-commonui.app | /mnt/diska/lib/commonui/commonui.so | /mnt/card/appprobe-default-commonui.txt |
| sysprobe-default-fusion.app | /mnt/diska/lib/fusion.so | /mnt/card/appprobe-default-fusion.txt |
| sysprobe-default-style.app | /mnt/diska/lib/style.so | /mnt/card/appprobe-default-style.txt |
| sysprobe-default-apconfig.app | apconfig.so | /mnt/card/appprobe-default-apconfig.txt |
| sysprobe-default-all.app | All four above | /mnt/card/appprobe-default-all.txt |

Start with apconfig, then the other individual variants, then all four. Return
all logs and note which runs exited normally versus rebooted. Each first line
names its variant. `candidate handle` must be nonzero to test that dependency;
a failed load records dlerror and exits without invoking the getter.
`RETURNED default font getter` records its pointer and the following line
records the returned string (bounded by the existing log formatter). NULL is
logged as a probe failure. A non-NULL pointer/path establishes getter progress,
not font-file usability; these discovery probes do not create a font.

All variants load candidates before applib_init. The all-four variant follows
the calculator order: applib, commonui, fusion, GUI, style, apconfig, then init.
The individual variants keep their library in the same relative position.
Loaded dependencies survive through applib_quit; handles are closed during
cleanup. A candidate may load its own dependencies internally, so success does
not prove that library alone implements the getter. The tested direct-path
font probe remains available unchanged.

Both shell and CMake builds generate these probes. ELF and stub checks pass.
Host mocks cover all five variants on success, candidate load failure, logging
failure after init, and a NULL getter result. To run these checks from sysprobe:

```sh
for variant in 1 2 3 4 5; do
  cc -std=c11 -Wall -Wextra -Werror -DPROBE_UI=1 \
    -DPROBE_DISCOVERY="$variant" '-D_Static_assert(x,y)=' \
    -Wno-pointer-to-int-cast tests/discovery_flow.c probe.c -o /tmp/x7-discovery-flow
  for scenario in 0 1 2 3; do /tmp/x7-discovery-flow "$scenario" || break; done
done
```

# Direct-path font probe

The supplied `appprobe-font.txt` and user observation confirm font creation,
selection, visible text rendering, redraw on START, and normal START exit.
`gui_create_font("/mnt/sdisk/ATTFV1.TTF",16)` returned font ID 0x26cc;
default fontface selection returned 0. The two text calls returned 0x88 and
0x89 on both draws; their return-value meaning remains unverified. Cleanup
passed font destruction, GUI/applib close and shutdown, ending with DONE and
zero probe failures. The run lasted 432 ticks and exited for reason 1 (START).

The default-font getter now works when apconfig.so is loaded before applib_init;
the direct-path font sequence also remains independently verified. Previous UI logs are now archived under
`logs/appprobe-ui_*.txt` (relative to the workspace root).

**`sysprobe-font.app`** now has a console-verified font and text sequence. Install it as `calculat.app`
through the confirmed launcher route. It uses `/mnt/sdisk/ATTFV1.TTF` directly,
without calling the stalled default-font getter. It should display the verified
colored panel plus white **X7 font test** and **START exits** text. START and the
20-second timer retain their previous behavior after setup succeeds.

Return `/mnt/card/appprobe-font.txt` and report whether the text appears.
The log identifies font probe v1 and checkpoints font creation, selection,
text drawing, screen update and destruction. Text-call return values are logged
without treating an unverified return convention as pass/fail. Startup failures
use `/mnt/card/appprobe-font-start-error.txt`. Logs are overwritten on reruns.
If a font call stalls, the last BEFORE marker identifies it; the exit timer is
installed after setup and cannot bound a stalled initialization call.

This variant calls `gui_create_font(path,16)`, then
`gui_dc_set_default_fontface(font_id)`; text rendering uses foreground white,
text mode 2, font size 16 and two separate signed-16-bit rectangles, with
alignment/option 1/0 matching calculator callers. Window deletion precedes font
destruction during cleanup. The original console-verified `sysprobe-ui.app`
remains the font-free v4 color/button probe. Both build paths produce all three
applications: filesystem, UI and font.

The sdisk font has a standard TrueType SFNT directory with 12 tables and name
records identifying WenQuanYi Zen Hei, Medium. It is 11,003,096 bytes.
`CONFIG.BIN` stores DEFAULT_FONT with `attfv1.ttf` at offset 0x6ae7 (lowercase);
the copied filename is uppercase. `FFT.KO` contains FreeType routines and
recognizes both `.ttf` and `.TTF`. GUI.SO, FONT2.SO, FFT.KO and APCONFIG.SO match
the earlier binaries byte-for-byte. These facts support a direct font-loading
experiment; they do not establish why the default-font getter stalled.

Host font tests include the normal START/timer paths and font creation or
selection failures. Run the existing host-test command below with an additional
`-DPROBE_FONT=1` and scenarios 0 through 7 for this variant. The console run described below verifies this rendering sequence.

# Screen and button probe

The v3 console run confirms a visible 240×200 black panel, 31 extended key
callbacks, a timer exit (reason 2), successful GUI/applib cleanup and zero
probe failures. Key events included SDK Up/Down, L/R, A/B/X/Y and Select.
Their types were DOWN (0x20000000) and SHORT_UP (0x04000000), with one
zero-mask/zero-type event also recorded. Physical labeling remains based on
the SDK constants, not a deliberate physical-button mapping test.

v4 corrects the color state used for clearing: `gui_dc_set_background_color`
(group 3 index 0x5b), recovered from ebook.app, replaces foreground set_color
before clear_rect. The black panel in v3 is consistent with clearing using the
untouched background color. The user confirmed v4 color rendering works and START terminates the application.
The log also establishes that get_msg invokes GUI callbacks and the timer
internally: it returned zero after timeout, with zero outer messages dispatched.

**UI v4 is console-verified for colored rectangles, button delivery,
START exit and timer exit.** Default-font discovery is verified with apconfig.so; direct-path text rendering is verified in the font variant.

**Current UI v4 is a font-free diagnostic.** It draws a dark 240×200 panel
with an orange inset, which changes to green on an extended key event. There
is no text in this version. START still requests exit, and a 20-second timer
is installed after drawing succeeds. Each drawing stage has synced markers.

The v2 console log confirms pathless GUI loading returned a handle, but stops
before the default-font-path call returned. No window or exit timer was reached.
The observed restart after about 20 seconds is consistent with watchdog recovery;
its precise cause is unconfirmed. v3 skips all font calls to isolate the GUI
window/drawing/input APIs. The font getter may depend on omitted calculator
initialization or differ in this firmware; matching a named stub alone did not
establish that it was usable. The user confirms gui.so resides under /mnt/sdisk.


UI **v2** fixes the GUI loader name: it uses `dlopen("gui.so", 1)`, matching
calculat.app, instead of the guessed `/mnt/diska/lib/gui.so`. The supplied v1
console log showed a NULL GUI handle and orderly cleanup; no window or drawing
call was reached. v2 also logs `dlerror()` text on a failed GUI load. The log's
first line identifies the UI version.

The new binary is **`sysprobe-ui.app`**. Install it as `calculat.app` using the
launch route that worked previously. It aims to show the colored panel described above and write key diagnostics to
`/mnt/card/appprobe-ui.txt`. Press buttons to test them; **START exits**.
A firmware timer requests exit after **20 seconds**. The loop also caps total
messages at 256 and key events at 128. The timer and UI sequence is console verified;
its time limit still depends on the firmware delivering the callback.

Please return the log and describe whether the colored panel appeared, which buttons you
pressed, and whether the tools menu worked after exit. The last synced BEFORE
marker identifies the next call if execution stops. Startup errors use
`/mnt/card/appprobe-ui-start-error.txt`. These diagnostics are overwritten on
subsequent runs. The UI variant creates no filesystem scratch file.

It retains the console-tested process startup and applib init/quit sequence.
It loads gui.so, creates and focuses a window, draws colored rectangles,
registers a system-message handler, and
runs get_msg/dispatch_msg. Cleanup kills the timer, unregisters the handler,
deletes the window, closes GUI, then performs applib
shutdown and process exit. The timer uses the same set_timer API employed by
manager.app; no additional watchdog is installed or disabled.

The log records the first 64 GUI messages (ID, handles and raw data word),
the first 32 outer app message types, and up to 128 extended key events
(mask, event type and ticks). START is SDK mask 0x8; only extended-key message
13 is interpreted as a key pointer, matching calculat.app. Other messages go
to the vendor default callback. Exit reasons are 1=START, 2=timer,
3=application quit, 4=key limit; zero means the message loop returned or reached
its message limit without one of those requests. These values and physical
button mappings still require console confirmation.

Both CMake and `build.sh` build the UI variant alongside the original file
probe. Sources are `ui_probe.c`, `ui_abi.h`, and `ui_stubs.S`; the build checks
all 24 UI stubs against calculat, calibrat, ebook and manager binaries,
as well as existing ELF/runtime checks. Host control-flow tests cover START,
timeout, and font/window/timer creation failures and GUI load failure. Run them from this directory:

```sh
cc -std=c11 -Wall -Wextra -Werror '-D_Static_assert(x,y)=' \
  -Wno-pointer-to-int-cast tests/ui_flow.c ui_probe.c -o /tmp/x7-ui-flow
for scenario in 0 1 2 3 4 5; do /tmp/x7-ui-flow "$scenario" || break; done
```

The host build disables target layout assertions because host pointers differ
from MIPS O32. The actual MIPS builds enforce them. These mocks check control
flow and rectangle coordinates, not firmware rendering or timer behavior.

## Original standalone filesystem probe

`sysprobe.app` is ready to try through the launcher's application launch path.
It writes `/mnt/card/appprobe.txt`, exercises a few confirmed filesystem calls,
and calls the firmware application shutdown routine before process exit.
It has no graphical interface, input handling, or wait loop. A brief
blank screen or no visible change may be normal; the SD card log is the result.

Copy `sysprobe.app` to the console and launch it as an application using your
existing launcher mechanism. If the launcher requires a configured filename,
install a copy under that filename. Merely placing a file on the SD card might
not make the firmware list it; discovery/menu configuration has not been
established by this probe. On this console, the browser menu did not launch it, but replacing
`calculat.app` did. Use that confirmed route for the next run.
No supplied firmware app has been modified here.

On success the log includes the worker PID, process pointer, argument count,
libc_fs and applib handles, write/tell/negative seek/fstat results,
`RETURNED applib_init`, `RETURNED applib_quit`, `DONE`, and
`BEFORE group-7 exit(0)`. The expected stat size is 32 bytes. A clean return to
the launcher is also part of the test: the last marker alone cannot prove exit
finished. Return the log and report whether the launcher recovered normally.

Startup failure after a runtime call returns is written to
`/mnt/card/appprobe-start-error.txt`, with a stage number:

| Stage | Operation |
| --- | --- |
| 1 | Allocate process ID |
| 2 | Set process path/argv/envp |
| 3 | Obtain process object |
| 4 | Initialize thread attributes |
| 5 | Set explicit scheduling |
| 6 | Set joinable state |
| 7 | Create worker thread |

Only a failure rewrites the startup-error log; an old copy can survive a
successful run. No log can mean loader/launcher rejection, missing group-7
runtime, failure to open the SD log, or a trap that never returned. The first
probe does not log during the scheduler-locked startup interval.

Logs are overwritten on each relevant run. The only scratch path is
`/mnt/card/appprobe-work.bin`: existing content is preserved and causes a skip.
Creation requires the verified ENOENT result -2. The owned scratch file is
removed on normal cleanup; an interrupted run can leave it behind. Work and
write retries are bounded. One-tick checkpoint yields occur in the worker;
the probe neither disables nor registers an extra watchdog.

## Recovered application conventions

The supplied apps are fixed-address ELF32 little-endian MIPS O32 ET_EXEC files,
not Linux executables or relocatable shared libraries. They have two PT_LOAD
segments (RX code, RW data/BSS), `_init` and `_fini`, constructor/destructor
sentinels, and a separate ELF entry named `__start`.

`calculat.app` contains symbols and DWARF. Its `__start` at `0x6580739c`
takes **path, argv, envp**, not argc/argv. It calls `_init`, locks scheduling,
allocates a process ID, links it to the current parent, copies its environment,
and installs an executable-release callback at process offset 8. It initializes
36-byte thread attributes, changes the creating thread's PID temporarily,
creates the application's worker, restores the caller's PID, destroys the
attributes, unlocks, and returns status to the loader. The same sequence occurs
in `manager.app` at `0x600019f0`.

The worker (`process_start` at `0x65807290` in calculat.app) obtains its process
object, reads argv at offset 96, loads `libc_fs.so` with flag 2 (retrying once),
runs main, and calls group-7 exit (`0x70082`). This probe follows that startup
and termination sequence. Only the accessed process prefix is described; the
complete runtime object remains opaque. Thread-attribute size and process
offsets are confirmed by the vendor DWARF as well as disassembly.

The 15 additional runtime stubs belong to **group 7 in LIBC_SYS.KO**, not the
two SYSCFG tables in the root syscall library. They live in `app_stubs.S` and
`app_abi.h`, and are checked byte-for-byte against calculat.app. Root group-1
and group-2 wrappers provide filesystem and OS calls.

The v2 worker additionally loads `/mnt/diska/lib/applib.so` with flag 1,
calls group-18 `applib_init(argc, argv, NULL)` (0x120036), runs the probe,
calls `applib_quit()` (0x120037), and closes its applib handle before process
exit. These calls match calculat.app at 0x65802090 and 0x6580212c and its
`_lib_deinit` cleanup. No return-register meaning is assumed for init/quit,
because the inspected calculator does not use their return values.

Version 1 successfully started a process/thread and passed every file test,
but its missing application lifecycle calls left the tools menu unresponsive
after the final exit marker. Manager code registers the application before
execvp (0x60000898/0x60000a48). The omitted lifecycle handshake is the leading
explanation, rather than a confirmed fault inside the process-exit API.
The applib.so implementation is not in the supplied binaries, so its internal
notification behavior has not been traced. The v2 console run confirms that
adding this lifecycle sequence restores normal exit on the tested console.

This probe still omits resource loading, graphical scenes, and message loops.
Launching, worker execution, and a clean return to the tools menu are now
console-verified when installed as `calculat.app`. Browser-menu launching did
not work and is not claimed to be supported. The previous binary is retained as
`sysprobe-v1.app`. The log identifies the new binary as v2.

If init returns, cleanup always calls quit even when logging fails.
`BEFORE applib_quit` without `RETURNED applib_quit` identifies quit or the
intervening yield as the next investigation point. If all lifecycle markers
return but keys remain ineffective after exit, provide the log and menu
behavior; more manager/message-loop analysis will then be needed.

## Address and rebuilding

The default code base is **0x69800000**, with an ELF entry pointing to **__start** (its offset depends on link order). This lies in the gap between supplied applications at 0x69400000 and
0x69c00000. The checker rejects overlap with any supplied firmware ELF load
segment. This establishes compatibility with the provided binaries' ranges,
not proof that every unseen firmware variant leaves that address free.
Renaming the file does not change its load address.

Configure this package independently from the workspace root:

```sh
cmake -S sysprobe -B build-sysprobe
cmake --build build-sysprobe --parallel
```

The output is `build-sysprobe/sysprobe.app`. Set `-DAPP_BASE=0x........` at
configure time to change the base. The MIPS cross toolchain is selected
automatically; CMake 3.20+ is required. Shared sources and validation inputs
remain at the workspace root. See [workspace build instructions](../README.md).

The original shell build remains available and writes outputs here:

```sh
sysprobe/build.sh
```

`CC` defaults to `mips-linux-gnu-gcc`. If a different available application
address is established, rebuild with `APP_BASE=0x........`; the checker still
rejects overlaps with the supplied binaries. The app has no undefined symbols,
libc link dependencies, dynamic relocations, or GUI resources. The build
produces `sysprobe.dis` and `sysprobe.elfstruct` for inspection.

Validation includes all 243 root syscall stubs, the 15 vendor process/thread stubs and two group-18 lifecycle stubs,
ELF entry/segments/BSS and address ranges. A host mock also checked the CRT's
success path and seven startup failure stages for scheduler unlock, PID
restoration, and resource cleanup. That control-flow test does not emulate
firmware syscalls or prove launcher behavior. Host mocks also checked worker
cleanup on success, a missing applib library, logging failure after init, and
a failed initial log open.

## Version 2 console result

The supplied appprobe.txt identifies v2 and records both lifecycle calls
returning, applib dlclose returning zero, all six filesystem assertions passing,
zero assertion failures, DONE, and the marker before process exit. The worker
runs for 41 ticks (0xd1ad to 0xd1d6). The user observed a clean exit this time,
which supplies the exit evidence the final log marker alone cannot provide.

The tested standalone sequence is therefore: loader __start → process/thread
creation → worker → load libc_fs/applib → applib_init(argc, argv, NULL) → work
→ applib_quit → close applib → group-7 exit(0). This establishes a working
minimal app on this console without a graphical scene or message loop.

Framebuffer dodecahedron demo: `sysprobe-dodeca-fb.app`

Run this using the same calculator replacement procedure as the bitmap demo.
It renders the same camera, lighting and rotating object into full 480×272
16-bit frames, then submits addresses directly to `/dev/fb`. START exits;
otherwise it exits after 30 seconds or 1200 frames, whichever comes first.
Diagnostics go to `/mnt/card/appprobe-dodeca-fb.txt`, with a separate
`appprobe-dodeca-fb-start-error.txt` for startup failures. Please preserve the
whole log, including the framebuffer setup and cleanup results. Whether the
image fills the display, its colours, and whether the menu returns normally
are useful observations for the framebuffer visibility test.

Build with `./build.sh`, or CMake target `sysprobe-dodeca-fb`. The original
`sysprobe-dodeca.app` remains the GUI bitmap version for comparison. Frame
count divided by `animation elapsed ticks` measures frames per OS tick;
`total render ticks` and `total fb submit ticks` separate CPU drawing from
buffer submission. No per-frame card writes are performed. Both versions
use the same 10 ms timer and one-tick yield; this test retains that scheduling
limit to isolate the display path. The framebuffer version clears and renders
a whole screen each frame, whereas bitmap v2 updates a centered 240×224 area.

The host mock `tests/dodeca_fb_flow.c` exercises START, timeout and the existing
GUI/timer/submission failure paths, and asserts that video stops before its
pool is freed. It needs a 32-bit host libc to compile directly; the development
host ran it with temporary host-width ABI adaptations, while the real app's
MIPS ABI is checked by `check_app.py`.

Framebuffer v3 adds the emulator's full-screen blending-region setup and
display activation call. V2 rendered and submitted successfully but showed
only the dark GUI background. V3 logs creation, enabling, activation, disabling
and destruction of the blending region as well as the existing timing data.

Framebuffer v3 is console verified: smooth animation and correct appearance,
990 frames before the 30-second timer, and successful display/pool cleanup.
Bitmap v2 produced 617 frames on its corresponding run. These are submitted
frame counts, not measured panel scanout rates.

Third demo: `sysprobe-dodeca-fast.app`

This separate variant uses the working framebuffer setup, enlarges the object
by 175% in projection (focal length 525 instead of 300), and removes the
explicit one-tick sleep per frame. Its frame timer requests 1 ms rather than
10 ms. Each callback renders up to four frames, returning sooner once one OS
tick has elapsed, so the message pump can service keys, timeout and runtime
housekeeping. Actual timer granularity is firmware dependent. Face filling
uses scanline spans with paired 16-bit pixels in 32-bit stores; full-screen
background clearing also uses 32-bit stores. The five-buffer ring is retained.

Run it using the calculator replacement procedure. START exits, with a
30-second timeout and a 12,000-frame guard. Log:
`/mnt/card/appprobe-dodeca-fast.txt` (separate `-start-error.txt` on startup
failure). Use `measured loop frames` and `measured loop ticks` for comparison;
these delimit the message-loop interval without startup/card logging. The
render/submission totals exclude the first demonstration frame in this
variant. Frame counts measure submissions, not unique panel scanouts. In fast v2, rotation follows elapsed OS ticks (one phase step per three ticks),
so higher rendering throughput no longer speeds the object's rotation.

Build with `./build.sh` or CMake target `sysprobe-dodeca-fast`. Host checks
render 1200 rotations under AddressSanitizer/UndefinedBehaviorSanitizer and
verify the enlarged object leaves all four screen borders clear. The
framebuffer lifecycle mock also supports `PROBE_FAST`. Preview:
`dodeca-fast-preview.png`. Fast v1 is console tested: 1471 measured loop frames in 1180 OS ticks,
with an initial burst reported by the user; cleanup succeeded. Fast v2 adds
time-based rotation and 100-tick frame-count buckets to investigate that burst.

Fast v2 keeps the same file name and diagnostic path. Frame buckets are stored
in RAM during animation and written after video shutdown; each bucket index
and count appears as a pair of log entries. The final bucket may cover only
part of 100 ticks. These measure submissions, not the display's refresh rate.

Fast v3 investigates the startup noise band seen in v2. It queries the
framebuffer driver's current video addresses before each render and chooses a
buffer outside those addresses and the two most recently submitted buffers.
Rendering cadence and time-based rotation are retained. New log counters:
`fb protected slot skips`, `fb no available buffer`, and
`fb address query failures`. This guard is console verified to remove the observed noise band; the
address query remains a software snapshot rather than a documented scanout
completion fence.

Fast v3 is console verified: constant-speed enlarged rotation and no startup
noise band. The final run rejected 3683 protected buffer candidates, with no
unavailable buffers or query failures. Display shutdown and pool freeing
succeeded; no assertion failures. This is the latest working fast demo.
