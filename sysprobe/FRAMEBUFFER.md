# X7 framebuffer and video presentation

The working native video path is implemented in [fb_video.c](fb_video.c) and
[fb_video.h](fb_video.h). The latest console-verified example is
`sysprobe-dodeca-fast.app` **fast v3**, with guarded buffer reuse, constant
rotation and a 175% projection scale. The slower `sysprobe-dodeca-fb.app`
**framebuffer v3** remains available for comparison. These version numbers
refer to two different binaries. [GUI.md](GUI.md) describes input/lifecycle;
[DEMOS.md](DEMOS.md) records results and timing limitations.

This is the Actions vendor `/dev/fb` ioctl ABI, not Linux fbdev. Do not use
Linux framebuffer structs, ioctl numbers or assumptions about mmap here.

## Recovered emulator path

`emulator.app` opens `/dev/fb` at `0x688097e0`, switches game/UI state at
`0x68809b1c` / `0x68809b60`, and uses video/blending ioctls for game frames.
Its GUI layer supplies menus and overlays. LIBEMU.SO output initialization
at `0x50801360` allocates `0x385800` bytes through group-1 slot 114 with flag
**10 decimal (`0x0a`)**. Five slots rotate modulo five at `0x50800d70`, each
`0x96400` bytes (`640*480*2+1024`), with a sixth scratch slot.

At `0x50800cd8`, group-8 API 2 command 13 supplies source size/format; changed
parameters go through ioctl `0x5012`, and a slot's translated address goes
through `0x4667`. Group-1 slot 116 translates the pointer. Command 14 at
`0x50800c98` supplies the current raw slot pointer to the emulator core. There
is also a row copy/conversion path at `0x50800f78`, so end-to-end zero-copy is
not established. The driver has scaling/DMA/output code, but no measured
emulator FPS or recovered general GPU renderer follows from that alone.

## Device calls and structures

Open with `sys_open("/dev/fb",2,0)`. Descriptors are opaque: logged values are
not small Linux-style fd numbers. Use the root group-1 `sys_ioctl` wrapper
(slot 97): `sys_ioctl(fd,command,(x7_word_t)argument_pointer)`. Zero was the
successful return in the working tests; the helper treats negative values as
failure. Do not dereference the translated video addresses on the CPU.

| Command | Buffer / argument | Recovered use |
| --- | --- | --- |
| `0x4662` | output u32[11], 44 bytes | Query LCD/general parameters |
| `0x4675` | input u32[9], 36 bytes | Create the emulator's full-screen blending region |
| `0x4676` | s32 list `{0,-1}` | Enable region 0, terminated by −1 |
| `0x5012` | input u32[11], 44 bytes | Set video source parameters |
| `0x4660` | NULL | Start video |
| `0x4666` | NULL | Activate/update display path, as used after emulator video start |
| `0x4667` | input address[3], 12 bytes | Submit video plane addresses |
| `0x5011` | output address[3], 12 bytes | Read driver's current video address triplet |
| `0x4661` | NULL | Stop video and return toward UI output |
| `0x4677` | s32 list `{0,-1}` | Disable the blending region |
| `0x4679` | NULL | Destroy/clear blending state, matching emulator cleanup |

The current LCD query returns:

```text
{480,272,16,960,5,11,6,5,5,0,1}
```

Words 0/1 are dimensions and word 2 agrees with 16-bit depth; word 3 equals
width*2 but its complete semantics and words 4–10 remain unassigned. The LCD
block is **not** the video-mode block even though both have 11 words.

The tested video parameters are:

| Word / byte offset | Value | Interpretation |
| --- | --- | --- |
| 0 / 0 | 480 | source width |
| 1 / 4 | 272 | source height |
| 2 / 8 | 480 | source stride in pixels |
| 3–6 / 12–24 | 0 | unresolved fields, zero in this setup |
| 7 / 28 | 1 | packed 16-bit video format, visually verified |
| 8 / 32 | 0 | unresolved; vendor explicitly initializes it to zero |
| 9–10 / 36–40 | 0 | unresolved fields, zero in vendor BSS-backed block |

LIBEMU accepts format 1 or 8 from its core, but this project tests only 1.
[GUI.md](GUI.md#color-words-and-native-pixels) gives the exact pixel packing.
Mode 1 produced the expected image with that packing; format 8, YUV planes,
scaling modes and arbitrary source dimensions are untested. Submission is
`{translated_buffer_address,0,0}`; initialize all three words because the
driver copies 12 bytes, even when only one packed plane is used.

Additional traced calls are **not used by the demos**: `0x5013` accepts a
scalar aspect/output mode (LIBEMU uses variants including 3); `0x4663` reads
the GUI framebuffer address; `0x4664` changes dimensions/allocations;
`0x5014` changes selected LCD parameters. Their broader contracts and safe
state restoration are not established. The working probe does not require
these or a general framebuffer mmap ABI.

## Pool allocation and address translation

| Group-1 slot | Wrapper | Tested usage |
| --- | --- | --- |
| 114 | `x7_syscall_114(bytes,flags)` | pool allocation; flag `0x0a` |
| 115 | `x7_syscall_115(pool)` | pool release after video stops |
| 116 | `x7_syscall_116(cpu_pointer)` | translated address for driver submission |

For native frames the helper allocates five slots with stride
`480*272*2+1024 = 262144 = 0x40000`, total `1310720 = 0x140000` bytes.
Pixels occupy the first 261120 bytes of each slot; 1024 bytes are padding.
Rows themselves remain tightly packed. Initialize every pixel buffer before
video can read it. Slot 116 is called for each slot; do not infer translation
by subtracting a constant, even though one console run mapped CPU
`0x13c80000` to driver `0x03c80000` and subsequent addresses advanced by
`0x40000`. Mapping/cache/coherency semantics beyond the tested path remain
unresolved. No explicit cache flush is used by this working example.

## Working initialization and shutdown

The source performs this exact initialization sequence:

1. Initialize the app/GUI window and focus, clear it and commit GUI drawing.
2. Open the framebuffer, query and require 480×272, allocate and initialize
   the five-slot pool, and obtain translated addresses.
3. `0x4675({0,0,0,0,0,0,480,272,0})`, then `0x4676({0,-1})`.
4. `0x5012({480,272,480,0,0,0,0,1,0,0,0})`.
5. `0x4660(NULL)`, then `0x4666(NULL)`.
6. Render and submit a frame, then service animation and input in the GUI
   message loop. Subsequent video frames require no GUI bitmap/update calls.

Shutdown kills both timers and unregisters the system dispatcher, stops video
with `0x4661`, waits three OS ticks after a successful stop, disables blending
with `0x4677`, destroys it with `0x4679`, frees the pool, closes `/dev/fb`,
deletes the window and closes GUI, then completes normal app shutdown.

The helper marks start/blending attempts before calling the driver so failure
cleanup still attempts their inverse. If video stop fails, it retains the pool
to avoid freeing memory potentially in use, and reports failure. This protects
against a dangling buffer; it does not guarantee recovery from driver failure.
The demo is entered from the tools menu with video presumed inactive. It does
not snapshot/restore another application's active video mode and is not a
tested shared-video ownership protocol.

## Buffer ownership: the startup noise fix

A five-buffer ring alone is insufficient. Fast v2 submitted 348/396/380 frames
per 100 ticks in the first three buckets, then settled to 100 per bucket.
A moving horizontal noise band appeared during startup. Fast v3 uses
`fb_video_acquire` **before modifying pixels**:

1. Read the current address triplet with `0x5011`.
2. Starting at the preferred slot, search all five slots for one whose
   translated address matches neither a nonzero current plane nor either of
   the last two successfully submitted addresses.
3. Render only into that selected slot, then submit it. After successful
   submission, advance the two-address recent history.
4. If no slot is available, skip rendering this callback iteration. A failed
   address query requests application exit; it does not fall back to blind reuse.

The console test resolved the noise, kept constant rotation and returned
cleanly. It recorded 3683 rejected candidate selections, zero unavailable
buffers and zero query failures. Those counts are candidate checks, not dropped
frames, displayed frames or measured tear events.

The query returns a **software state snapshot**, not a documented hardware
completion fence. At FB.KO `0xc2c04bb0`, pending addresses at device+0xe4 are
copied to current addresses at +0xf0 during display processing. Excluding
current and recent submissions protects the observed delayed-read case, but
exact latch timing, in-flight DMA ownership and guaranteed tear-free scanout
remain unknown. Keep the guard despite the performance cost; do not interpret
successful submission as permission to overwrite a buffer immediately.

## Evidence addresses and tests

| Binary / runtime address | Evidence |
| --- | --- |
| LIBEMU `0x508013cc` | allocation flag is decimal 10, not hexadecimal 0x10 |
| LIBEMU `0x50801e00` | width/height/stride/format initialization and video start |
| LIBEMU `0x50800cd8` | ring address conversion, mode updates and submission |
| emulator `0x688098c0` | whole-screen blending setup |
| emulator `0x68809a0c` | video start followed by `0x4666` |
| emulator `0x688099a4` | blending disable/destroy cleanup |
| FB.KO `0xc2c04a5c` | copies 44-byte video parameter block to device+0xb8 |
| FB.KO `0xc2c04a9c` | copies 12-byte pending plane block to device+0xe4 |
| FB.KO `0xc2c06908` | current plane-block getter, 12 bytes from device+0xf0 |
| FB.KO `0xc2c070fc` / `0xc2c061c0` | video start / stop |
| FB.KO `0xc2c06f5c` | submission, including conditional synchronization |

These are runtime addresses in the supplied files, not universal firmware
addresses. `check_app.py` verifies generated app layout/stubs. Host rendering
runs under ASan/UBSan; lifecycle mocks cover shutdown/failure paths, a fixed
current address while submissions advance, recent-buffer exclusion and query
failure. Host mocks do not simulate the LCD, DMA or scheduler. Actual visibility,
noise removal and menu return are established by the console tests.

Open questions include explicit completion/vblank synchronization, display
refresh rate, the startup burst's scheduling cause, tick/wall-time relation,
cache mapping semantics, unknown mode fields, and which of blending enable
versus display activation is individually necessary. Adding both made the
previously invisible video work; they have not been isolated experimentally.
