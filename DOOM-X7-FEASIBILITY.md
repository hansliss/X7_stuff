# DOOM on the Action X7: feasibility assessment

Current checkout: source-only; rebuildable binaries are not bundled. The games
now have independent launcher entries and console-confirmed resource loading
from `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/`; Calculator is restored.
Earlier binary/path/test directions below are historical. Current instructions
are in the game READMEs and root SESSION.md.

Assessment based on the local linuxdoom-1.10 source, DOOM.WAD, recovered X7
ABI, and console demo results. A silent native-resolution prototype has since
been built and successfully run on the console; see
[doom_x7/README.md](doom_x7/README.md). Its full-output DEMO1 run achieved an
estimated 88 rendered/submitted frames per second at unscaled 320×200, about
2.52 times the real-time 35-tic/s requirement for that scene. At the time of this assessment, broader gameplay
and combined gameplay/audio performance remained unmeasured. The final DOOM
port was later console-verified with music/effects and interactive gameplay;
see the current PLAY.md for those results. The separate
[PCM audio probe](sysprobe/AUDIO.md) has now completed successfully with audible
sound through the single speaker and clean audio/application teardown.

## Source and data checks

The supplied WAD is an IWAD, 11,159,840 bytes, with 2,194 lumps and maps
E1M1–E3M9. Its directory and every lump fit within the file. MD5:
`1cd63c5ddff1bf8ce844237f580e9cf3`. This is structurally consistent with the
three-episode game; this audit does not independently authenticate its release.
It can be read on demand rather than loaded entirely into memory.

Important compatibility issue: `doomdef.h` sets VERSION=110, while all three
embedded demos start with 109. `G_DoPlayDemo` rejects these demos unchanged.
For the benchmark, explicitly support and validate v1.9 demo playback (or use
a matching recording). Simply leaving the existing version check will fail.
Savegame compatibility is a separate concern; use new X7 save files.

An exploratory cross compilation compiled all 61 Makefile translation units
apart from `i_video.c`, and successfully combined their objects with `ld -r`.
A second audit compiled and combined the 57 core units with all five Linux
`i_*` implementation files excluded, using freestanding/no-builtin flags.
This verifies basic MIPS C compilation and exposes external dependencies;
it does not establish a working port or a firmware-compatible executable.
The installed Linux headers insert glibc/time64 symbols. Soft-float compilation
against those headers fails because `gnu/stubs-o32_soft.h` is absent. The audit
therefore used the compiler's default floating-point ABI; production code needs
its own small runtime headers and appropriate target flags, not Linux libc.

## Operating system and runtime dependencies

DOOM's OS boundary is mostly `i_video.c`, `i_system.c`, `i_sound.c`, and
`i_net.c`, but file operations and stdio also occur inside the core.
The old Makefile links X11/Xext, nsl and libm; replace it with an X7
freestanding app target and the existing application CRT.

| Source / dependency | Existing use | X7 approach and evidence |
| --- | --- | --- |
| `i_main.c`, `i_system.c` | main, exit, error handling | Existing `sysprobe/crt.c`, process worker, load libc_fs/applib, applib init/quit, group-7 exit; verified clean launcher return |
| `w_wad.c`, `m_misc.c`, `m_menu.c` | open, close, read, write, lseek, fstat | Group-1 slots 68/69/70/71/72/80 have wrappers and console filesystem evidence |
| WAD discovery, screenshots | access; environment/path discovery | Explicit `/mnt/card/.../DOOM.WAD` path, bounded open/stat existence checks, explicit config/save paths |
| `d_main.c` | getenv, optional mkdir, response-file reading, terminal input | Supply paths directly; omit unsupported command-line modes or implement their file operations; remove blocking terminal prompts |
| `m_misc.c` | fopen/fclose, fscanf/sscanf/fprintf for defaults | Implement bounded config parsing/writing on X7 files, or fixed defaults in the first prototype |
| Core diagnostics / optional dumps | printf/fprintf/sprintf, FILE streams | Small formatter and SD logger; preserve string formatting used to construct lump names, avoid per-frame SD logging |
| `i_system.c` | gettimeofday, usleep, optional sleep | Calibrated monotonic clock → DOOM tics; bounded GUI timer callbacks and scheduler cooperation |
| `i_video.c` | X11 display/input/palette, SysV shared memory, signal/getuid | Replace the entire backend with GUI button events plus `fb_video.c`; no X server or shared-memory emulation needed |
| `i_net.c` | socket, bind, sendto, recvfrom, ioctl(FIONBIO), gethostname/gethostbyname, inet_addr, errno/strerror | Single-player initialization of doomcom; no Linux networking dependencies in the first port |
| `i_sound.c` | OSS /dev/dsp open/ioctl/write/close; optional popen sound server; sigaction/setitimer; pow | Silent backend built; native libsound.so PCM output and cleanup now console-verified by sysprobe-audio; DOOM mixer/music integration and combined performance remain untested |
| Heap | malloc, realloc; optional sound calloc | Implement a matched allocation family; candidate kmalloc/kfree slots 60/61 and realloc 62 need validation for this workload |
| Memory/string functions | memcpy, memset, strcpy/strncpy, strcat, strlen, strcmp/strncmp, strcasecmp/strncasecmp, toupper | Local implementations/ASCII helpers; compiler may also generate memory helpers |
| Numeric/formatting helpers | abs, atoi (compiled as strtol), sprintf, sscanf | Small runtime or direct substitutions; no dependency on the toolchain's Linux implementations |

The core object audit additionally exposes `fread`, `fseek`, `ftell`, `feof`,
`getc`, `setbuf`, `stdin`, `stdout`, `stderr`, and `exit`. These serve response
files, defaults, debug output, and terminal behavior, not just the platform
files. Either provide them or remove/replace their calling paths. The Linux
headers spell some symbols as `open64`, `lseek64`, `fopen64`,
`__fstat64_time64`, `__isoc99_fscanf`, `__isoc99_sscanf`, and
`__ctype_toupper_loc`; these are artifacts to eliminate with local headers,
not additional X7 APIs to recover. Re-audit undefined symbols after porting.

X7 filesystem compatibility needs explicit adapters:

- Descriptors are opaque, not small POSIX file numbers; use a local handle
  table if the engine needs POSIX-style validity tests.
- Native calls can return negative error codes, whereas DOOM commonly checks
  exactly -1. Normalize failures, and handle short reads/writes.
- X7 O_CREAT/O_TRUNC are 0x200/0x400; do not pass Linux flags directly.
- Seek offsets/results are 64-bit, with O32 argument placement handled by the
  recovered wrapper. X7 stat has a 64-bit size at offset 32; Linux stat differs.
- Use an explicit uppercase WAD filename and absolute paths. Do not rely on
  cwd, HOME, DOOMWADDIR, or case sensitivity.

## Display integration

`SCREENWIDTH=320`, `SCREENHEIGHT=200`. `screens[0]` is a tightly packed,
row-major 8-bit palette-index buffer: 64,000 bytes. `R_DrawColumn` and
`R_DrawSpan` sample textures and lighting lookup tables into this buffer.
Menus, status bar, automap and wipes also assume indexed pixels. Keep this
representation; replacing it with native 16-bit pixels would spread changes
through the engine.

Replace `I_InitGraphics`, `I_ShutdownGraphics`, `I_StartFrame`, `I_StartTic`,
`I_UpdateNoBlit`, `I_FinishUpdate`, `I_ReadScreen`, and `I_SetPalette`.
`I_ReadScreen` still copies indexed pixels. In `I_SetPalette`, apply
`gammatable[usegamma]` and build 256 native entries:

```c
pixel = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
```

The first DOOM console run revealed swapped red/blue channels in the earlier
packing. DOOM audio v2 uses red at bits 11–15, green at 5–10 and blue at 0–4
(RGB565), matching the LCD component fields and the reported color swap.
Rebuild this table whenever DOOM changes its palette, including damage flashes,
item pickup effects and radiation suits. Lighting colormaps remain indexed.

In `I_FinishUpdate`, acquire a protected video slot, convert/scale indexed
pixels into it, and submit it. Reuse `fb_video.c`'s full initialization,
current/recent-address guard and shutdown, rather than writing a live LCD
buffer. Native output is 480×272×2 = 261,120 bytes. The existing five-slot
pool occupies 1,310,720 bytes. No GUI bitmap copy is required each frame.
The address query is not a documented scanout completion fence, but its guard
resolved the actual demo noise and should be retained.

The original backend's `-2`/`-3` expansion operates on X11 8-bit image data;
it cannot simply be reused for native video pixels. No hardware 3D support is
needed. Hardware source scaling is suggested by emulator/driver code but only
480×272 source mode 1 is console tested. Treat 320×200 input and aspect-mode
ioctl 0x5013 as separate experiments, not an established capability.

## Resolution and pixel doubling

| Option | Result on 480×272 | Assessment |
| --- | --- | --- |
| Original 320×200 at 1× | Center at (80,36) | Simplest first benchmark; preserves all UI but has small text and uncorrected 8:5 image aspect |
| Software scale to 362×272, approximately 4:3 | Center at (59,0) | Good target for the original game's intended display proportions; nearest-neighbor lookup maps avoid division per output pixel |
| Stretch to 480×272 | Full panel | Feasible software conversion; visibly wider than intended |
| Exact 2× original framebuffer | 640×400 | Does not fit; cropping or rescaling loses the benefit of exact doubling |
| Render 160×100, double to 320×200 | Fits | Possible renderer adaptation, but menus/HUD/fonts/patch coordinates need care; not a SCREENWIDTH/HEIGHT-only change |
| Render 240×136, double to 480×272 | Full panel | Possible with more extensive resolution/UI and aspect-ratio changes; unlikely to be the best first port |
| Existing low-detail renderer | Samples roughly half as many view columns, duplicates horizontally | Promising optimization, but broken in this checkout and needs repairs |

Low-detail caveat: `M_ChangeDetail` explicitly returns before resizing and says
the feature is unavailable. `R_DrawColumnLow` is marked faulty;
`R_DrawSpanLow` doubles both endpoint coordinates but advances/writes two pixels
per iteration while decrementing their physical difference one at a time.
This needs correction and bounds tests. Fuzz and translated columns have
detailshift-related hacks too. It is not a tested switch we can simply enable.
Horizontal duplication also does not halve vertical work or UI resolution.

## Proposed controls

Use GUI message 13 and its `ui_key` mask/type, translating state transitions
into `D_PostEvent(ev_keydown/ev_keyup)`. SDK names below need a deliberate
physical mapping check; START is verified. Proposed mapping:

| X7 button | Mask | Gameplay | Menu |
| --- | --- | --- | --- |
| Up / Down | 0x10 / 0x20 | Forward / backward (arrow keys) | Navigate |
| Left / Right | 0x40 / 0x80 | Turn (arrow keys) | Adjust |
| L / R | 0x100 / 0x200 | Strafe left / right (`,` / `.`) | Optional navigation |
| A | 0x400 | Fire (KEY_RCTRL) | Enter / confirm / yes |
| B | 0x800 | Use / open (space) | Escape / back / no |
| X | 0x1000 | Run modifier (KEY_RSHIFT) | Optional shortcut |
| Y | 0x2000 | Cycle owned weapons | Optional shortcut |
| Start | 0x8 | Open menu (Escape) | Close/back |
| Select | 0x4 | Automap (Tab) | Optional shortcut |

DOOM has numeric weapon selection rather than a next-weapon key. Implement
cycling through owned weapons using numeric key events or a small command
adapter; decide whether to include fist/chainsaw in the cycle. Context changes
must release the previously mapped keys so menu/game transitions cannot leave
fire or movement stuck. Treat LONG/HOLD as held state, not repeated toggles;
release on SHORT_UP/LONG_UP/HOLD_UP. Iterate mask bits for multi-button events
and verify simultaneous movement/fire/strafe and long presses on hardware.
The logs establish press/short-release delivery, not exhaustive rollover.
Provide fixed save-slot names or a button-driven name editor; keyboard text
entry, chat and cheats have no direct mapping. Exit through the quit menu and
the tested app cleanup, rather than making START immediately terminate DOOM.

## Memory and arithmetic risks

`I_ZoneBase` initially sets a 6 MiB zone, but the SNDSERV defaults subsequently
override it to 2 MiB. The prototype explicitly starts at that effective 2 MiB
default and completed all three supplied demos in host tests.
`V_Init` allocates four 64,000-byte
screens outside it; wipes, lump metadata and other allocations add overhead.
Zone + these screens + native video pool alone need about 3.49 MiB, before
code/BSS, stack, metadata, library allocations and temporary buffers. Free
contiguous RAM and allocator behavior have not been measured. An 11 MiB WAD
does not imply an 11 MiB heap requirement; cache lumps through the zone.

`W_AddFile` allocates the directory with alloca: this WAD needs 35,104 bytes
on the worker stack. The current CRT uses vendor-default thread attributes;
stack capacity for this workload is unverified. Move large temporary directory
allocations to the heap and measure stack headroom. Verify app load segments
against firmware ranges once code and BSS are linked; the probe's chosen base
alone does not guarantee room for a larger executable.

DOOM largely uses 16.16 arithmetic and precomputed trig tables, but
`m_fixed.c:FixedDiv2` has the integer implementation disabled and uses double
division. Replace it with checked integer division, using multiplication by
65536 rather than left-shifting negative signed values. Then inspect generated
code/runtime helpers; 64-bit integer division can still be costly.
`FixedMul` already uses a 64-bit product. Avoid x86 assembly options and check
signed overflow, alignment and aliasing assumptions before optimizing.

## What the dodecahedron tells us about speed

The fragment demo's recorded interval is 571 frames / 4040 OS ticks, with
3081 render ticks and 37 submit ticks. Thus about 76.3% of loop ticks were
measured in rendering and 0.9% in submission. The user estimated about 15 FPS
visually. Neither OS tick duration nor unique panel scanout FPS is established.
Do not convert these counters into a precise FPS or CPU MHz claim.

The enlarged flat demo establishes that native software rendering and display
can look smooth, but its small convex scene and startup submission bursts are
not a useful DOOM workload proxy. The latest guarded fast run recorded 3437
frames / 10450 loop ticks. Earlier nominal 30-second tests submitted 617 GUI
frames and 990 framebuffer frames; timing/pacing/rendering changed together.

DOOM's 64,000 indexed pixels are about 49% of the panel's 130,560 pixels.
Its inner loops use texture/light lookups and fixed-point increments, whereas
the fragment demo does ray-plane intersection, square roots and division for
each covered object pixel. That favors DOOM's pixel cost. Against this, DOOM
adds BSP traversal, wall/plane setup, sprite overdraw, game logic, collisions,
enemy AI, palette conversion and potentially audio. The dodecahedron shades
only its covered area; comparing full-screen pixel totals alone is misleading.

Judgment: a playable silent prototype is plausible and worth trying. There is
insufficient evidence to promise 20, 30 or 35 FPS. DOOM simulation runs at
35 tics/second (the I_GetTime comment claiming 70 is stale); it can draw fewer
frames while preserving real-time simulation if the CPU can execute those
tics. Scaling/doubling adds output work and does not speed the renderer unless
its internal resolution or detail work is reduced.

## Prototype and measurement order

1. Build a silent single-player X7 app with explicit WAD path, fixed defaults,
   verified lifecycle, heap/file adapters and integer FixedDiv2.
2. Show 320×200 at 1× in guarded native buffers; verify palette flashes,
   menus, held buttons, map transitions, saves and clean launcher return.
3. Calibrate the clock against measured wall time. Do not derive tick frequency
   from the nominal GUI timeout or the visual FPS estimate.
4. Validate v109 demos, then run repeatable timedemos using DEMO1–3. Record
   elapsed wall time, simulation tics, rendered/submitted frames, render and
   conversion/acquisition costs, and peak heap/stack use. Stock timedemo reports
   through I_Error; make that benchmark completion use controlled cleanup.
5. Compare normal timedemo, `-noblit`, and `-nodraw -noblit` to separate output,
   rendering and simulation costs. Preserve scheduler/message service between
   bounded frames; disabling output must not disable input or housekeeping.
6. Test intended-aspect software scaling, then optional hardware scaling and
   repaired low detail. Benchmark busy scenes, not only the first corridor.
   Add audio after the silent build's performance and memory are understood.

Primary local references: `syscalls.md`, `sysprobe/GUI.md`,
`sysprobe/FRAMEBUFFER.md`, `sysprobe/DEMOS.md`, `sysprobe/FRAGMENT.md`,
`appprobe-dodeca-fragment.txt`, and `DOOM/linuxdoom-1.10/*.c`.
