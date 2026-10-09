# Rendering demos and console findings

Use [GUI.md](GUI.md) for the window/text/button ABI and
[FRAMEBUFFER.md](FRAMEBUFFER.md) for driver calls and buffer ownership.
[UI-notes.md](UI-notes.md) retains the earlier experiment history.
This document records the rendering sequence and what its measurements prove.

## Build, run and collect results

All demos use the standalone application's verified CRT and tools-menu launch
conventions. Install the selected binary as `calculat.app` using the existing
replacement procedure. Browser-menu launch is not supported by the tests.
Each `.app` has fixed load base `0x69800000`; renaming it does not relocate it.

```sh
# From the workspace root:
cmake -S . -B build
cmake --build build --target sysprobe-dodeca-fast

# Or rebuild all app variants in the source package:
sysprobe/build.sh
```

CMake output is `build/sysprobe/sysprobe-dodeca-fast.app`; shell output is
`sysprobe/sysprobe-dodeca-fast.app`. Standalone package builds also work:
`cmake -S sysprobe -B build-sysprobe`, then build the chosen target there.
The **latest working fast binary identifies itself as fast v3** in its log.
Ready-built binaries, disassemblies and ELF reports are beside the sources.

| Binary / target | Display path | Console log under `/mnt/card/` |
| --- | --- | --- |
| `sysprobe-ui.app` | colored panel and button events | `appprobe-ui.txt` |
| `sysprobe-font.app` | direct font loading and ASCII text | `appprobe-font.txt` |
| `sysprobe-default-{candidate}.app` | default-font dependency experiments | `appprobe-default-{candidate}.txt` |
| `sysprobe-hsv.app` | full-screen 4×4 color tiles | `appprobe-hsv.txt` |
| `sysprobe-bitmap.app` | full-screen packed-pixel HSV bitmap | `appprobe-bitmap.txt` |
| `sysprobe-dodeca.app` | current GUI bitmap demo v2, centered 240×224 update | `appprobe-dodeca.txt` |
| `sysprobe-dodeca-fb.app` | native 480×272 video demo, framebuffer v3 | `appprobe-dodeca-fb.txt` |
| `sysprobe-dodeca-fast.app` | enlarged native video, optimized and guarded, fast v3 | `appprobe-dodeca-fast.txt` |

Default-font candidates are commonui, fusion, style, apconfig and all. They are
historical dependency tests; only apconfig and all made the getter work.
Known failed-candidate behavior is recorded in [GUI.md](GUI.md#font-api-and-dependency).
The drawing demos exit on START or a timer. Dodecahedron versions request a
30,000 ms timeout and have guards of 1200 frames (bitmap/base framebuffer)
or 12000 frames (fast). Separate `*-start-error.txt` files identify runtime
startup failures. Logs are synchronized to SD during setup/cleanup and input;
no routine rendered-frame SD writes occur.

## Renderer and scene

[dodeca_render.c](dodeca_render.c) and [dodeca_mesh.h](dodeca_mesh.h) implement
a regular dodecahedron: 20 vertices, 12 pentagons and 30 shared equal-length
edges. Coordinates use fixed point with 1024 units per world unit. A 256-entry
sine table rotates about three axes. Outward winding, back-face culling and
painter depth order work for this convex object. Each pentagon is triangulated
for software rasterization and outlined. No hardware 3D API, floating-point
runtime or host libc is linked into the app.

The camera is at **(3,2,6)**, looks at **(0,0,0)** with up **(0,1,0)**; the
point light is **(−2,3,2)**. Perspective focal length is 300 pixels in the
bitmap/base framebuffer demos and **525 pixels in fast**, making the projected
object **175% as large** while keeping the world-space geometry, camera and
lighting fixed. Flat per-face lighting is ambient 25% plus Lambert diffuse,
with squared-distance attenuation and a blue/teal face palette. A small light
marker is drawn only when its projection lands on-screen.

Camera inputs are bounded; degenerate direction/up are rejected. This small
renderer rejects frames with vertices too close to the camera rather than
implementing polygon near-plane clipping. The scene is chosen so it does not
need that clipping. The larger projection remains within all four screen
borders throughout 1200 tested rotation frames. Previews
[dodeca-preview.png](dodeca-preview.png) and
[dodeca-fast-preview.png](dodeca-fast-preview.png) are host renders.

The bitmap version updates a 240×224 box at `(120,24)`, with a solid background
on the rest of the 480×272 window. Video variants render full native frames.
Fast uses fixed-point edge walking to fill contiguous triangle scanline spans
and aligned 32-bit stores for pairs of 16-bit pixels, including background
clearing. GCC `may_alias` makes the mixed 16/32-bit accesses explicit. The
other variants keep the original incremental edge-test rasterizer.

## Pacing and constant rotation

Bitmap v2 and base framebuffer v3 use a 10 ms frame timer and explicitly yield
one OS tick per frame. Fast requests a 1 ms timer and removes that sleep.
Each fast callback renders at most four frames, returning earlier once one OS
tick has elapsed. This returns control to the message pump without an
unbounded render loop. One frame can itself take longer than a tick, so this
is a batch limit, not a hard real-time callback deadline. The actual firmware
quantization of timer requests remains unknown.

Fast v1 advanced rotation by submission count, so an initial rendering burst
made it visibly spin faster. Since fast v2, rotation phase is
`(OSTimeGet()-animation_epoch)/3`, with the epoch set just before message
processing. The startup frame uses phase zero. This decouples angular speed
from rendering throughput; it does not cap submissions and can render the
same phase more than once. Fast v3 retains this and adds the current/recent
buffer guard described in [FRAMEBUFFER.md](FRAMEBUFFER.md#buffer-ownership-the-startup-noise-fix).

## Console results by generation

Version labels below are per binary, not a single sequence shared by all apps.
Values are decimal conversions of the raw hexadecimal log records.

| Experiment | Frames | Recorded ticks | Observed result |
| --- | ---: | --- | --- |
| [HSV tiles v1](../logs/appprobe-hsv.txt) | static | worker lifetime 2541 | correct full screen; visibly slow startup |
| [HSV bitmap v1](../logs/appprobe-bitmap.txt) | static | generation 9 (8 deliberate yields), blit 1, update 0 | correct map; clean START exit |
| [GUI dodeca v1](../logs/appprobe-dodeca_0.txt) | 276 | render 249, blit 1064, update 17 | correct geometry/light; choppy; background color bands |
| [GUI dodeca v2](../logs/appprobe-dodeca.txt) | 617 | render 57, blit 1061, update 594 | much smoother with smaller update box and shorter timer |
| [Framebuffer v1](../logs/appprobe-dodeca-fb_0.txt) | 0 | no render | wrong allocation flag `0x10` returned NULL; clean early exit |
| [Framebuffer v2](../logs/appprobe-dodeca-fb_1.txt) | 994 | elapsed 3185, render 915, submit 0 | successful calls but dark screen; missing visibility setup |
| [Framebuffer v3](../logs/appprobe-dodeca-fb_2.txt) | 990 | elapsed 3581, render 915, submit 0 | blending/activation added; smooth visible image and clean return |
| [Fast v1](../logs/appprobe-dodeca-fast_0.txt) | 1471 loop + 1 startup | loop 1180, render 508, submit 0 | very fast initial spin then slower; START exit |
| [Fast v2](../logs/appprobe-dodeca-fast_1.txt) | 3774 loop + 1 startup | loop 3281, render 1307, submit 0 | constant spin; moving noise band during startup burst |
| [Fast v3](../logs/appprobe-dodeca-fast.txt) | 3437 loop + 1 startup | loop 10450 | constant spin, noise resolved; all cleanup succeeded |

The raw logs are evidence for submitted/rendered frames. User observations
provide evidence for visibility, smoothness, artifacts and normal menu return.
The initial framebuffer flag was corrected to decimal 10 (`0x0a`). For the
dark-screen run, adding both blending-region setup and `0x4666` activation
made video visible; their individual necessity has not been isolated.

Fast v2's successive 100-tick bucket counts begin **348, 396, 380, 126**,
then settle to **100** in each full bucket. The last partial bucket has 24.
This confirms a submission burst independent of the frame-based rotation
problem. Its exact scheduling cause is unresolved. Fast v3 avoided **3683**
protected candidate selections, with **zero** unavailable buffers and
**zero** address-query failures. The disappearance of the band supports the
buffer-reuse explanation, but does not prove a general hardware fence contract.

All visible dodecahedron runs recorded successful cleanup and zero assertion
failures. Failed setup stages are also retained in the history rather than
being treated as successful drawing tests.

## Reading timings correctly

`frames` includes the initial demonstration frame. Fast `measured loop frames`
excludes it; `measured loop ticks` brackets message-loop processing after setup.
Fast render/submit accumulators reset immediately before that loop. Address
acquisition is **before** the render timer, so fast v3's address-query overhead
is included in loop time but not in render or submit totals. Loop counters can
also include key logging during dispatch; they are not isolated CPU benchmarks.
`animation elapsed ticks` has a broader capture point and is not the preferred
fast measurement.

Frame buckets are 64 RAM counters with width 100 OS ticks; index/count pairs
are written after video stops. Index 0 starts at the animation epoch. The last
bucket may be partial, and `frame bucket overflow frames` counts submissions
after the covered 6400 ticks. `fb protected slot skips` counts rejected buffer
candidates, while `fb no available buffer` counts failed acquisition attempts.
Neither is a count of missed panel refreshes.

A recorded zero-tick submission/update does not mean zero execution time or
completed scanout. The OS tick's wall-clock conversion is not verified, and
fast v3's elapsed count differs substantially from earlier runs. Do not infer
visible FPS from submissions or assume every submission reaches the panel.
The earlier approximate 9/21/33 FPS figures used the requested 30-second timer;
they are nominal comparisons, not measured panel refresh rates. Bitmap v2 had
2.24 times v1's submissions per nominal run; framebuffer v3 had about 60% more
than bitmap v2, while rendering the whole screen. Changes to timer pacing,
frame size and rasterization confound isolated speedup claims.

## Validation and remaining questions

Host rendering checks verify topology, changing images and clear borders under
ASan/UBSan. Lifecycle mocks cover START/timeout and library, window, DC, timer,
submission and query failures. The buffer mock keeps the reported current
address fixed and checks exclusion of that address and two recent submissions.
ABI/layout checks validate the MIPS build against the supplied firmware.
Host mocks need 32-bit pointer ABI or temporary host-width adaptations;
disabling structure assertions alone is insufficient for ioctl pointers on a
64-bit host. No host test simulates actual scanout or watchdog timing.

Remaining questions are the driver completion/vblank contract, true panel FPS,
timer catch-up/quantization, tick-to-wall-time conversion, cache translation,
and the untested GUI/font/video modes catalogued in the two API references.
The current working buffer guard and bounded message-loop callbacks should be
preserved while investigating those questions.
