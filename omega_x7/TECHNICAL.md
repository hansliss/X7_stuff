# Omega Race implementation and findings

## Approach and source map

The ROMs contain Z80 programs and vector-generator data, not X7 machine code.
Running the original programs preserves gameplay, attract mode, scoring and
sound commands without reconstructing the game rules. This is a dedicated board
emulator, not a build of MAME. Sources and licenses are in
[vendor/README.md](vendor/README.md); exact ROM CRCs are in
[rom-manifest.txt](rom-manifest.txt).

| Source | Responsibility |
| --- | --- |
| `machine.c`, `machine.h` | CPU callbacks, board memory/I/O, interrupts, DVG, AY synthesis, audio ring and event scheduler |
| `vendor/fastz80/` | Adapted instruction-based Z80 core |
| `render.c`, `render.h` | Clipped intensity-valued software vector rasterization with quarter-coverage antialiasing |
| `platform.c` | Native lifecycle, raw input, audio queue, timers, display, logs and save orchestration |
| `controls.h` | Physical masks, action assignments and spinner step |
| `paths.h` | Shared resource directory for all ROMs and battery RAM |
| `nvram.c`, `nvram.h` | Checked battery-RAM loading and replacement writes |
| `runtime.c` | Freestanding `memset`/`memcpy` |
| `tests/` | Real-ROM host runs, unit checks, persistence faults and firmware mocks |

## Board model

The supplied unmerged set matches MAME's original `omegrace` set definition.
The board mapping was checked against the
[MAME driver](https://github.com/mamedev/mame/blob/master/src/mame/bally/omegrace.cpp).

| Device/region | Mapping or clock |
| --- | --- |
| Main Z80 | 3 MHz |
| Main ROM M7/L7/K7/J7 | `0000–3fff`, four 4 KiB chips in that order |
| Main work RAM | `4000–4bff` |
| Battery-backed RAM | `5c00–5cff` |
| Vector RAM | `8000–8fff` |
| Vector ROM E1/F1 | `9000–9fff`, two 2 KiB chips |
| Sound Z80 | 1.5 MHz |
| Sound K5 | 2 KiB mirrored through `0000–0fff` |
| Sound work RAM | `1000–13ff` |
| Two AY-8912 chips | 1 MHz each; six combined channels |
| Periodic interrupt | Every 12288 main clocks: 244.140625 Hz |

Main ports: `08` read launches a vector list; `09` read services the watchdog;
`0a` write resets the DVG; `0b` read reports busy. `10/17` read DIP switches,
`11/12` read controls, `13` writes LEDs/flip, `14` writes the sound command
latch, and `15/16` read inverted six-bit Gray-code spinner angles. Sound port
`00` reads/acknowledges the command latch; writes at `00–03` address/program
the two AYs. Periodic events assert main IRQ and sound NMI; a command holds
sound IRQ until the latch is read.

The upright one-player DIP/input configuration supplies the original game's
coin/start behavior. Left/right adjust the 64-position spinner, not keyboard
events inside the ROM. ROM writes are ignored; unmapped reads return `ff`.

## CPU scheduling and performance decision

The first cycle-stepped Z80 implementation ran at 25% speed on the X7.
V3 switched to Nicolas Allemand's instruction-based core and scheduled events
instead of iterating over every CPU clock. Both clock rates, AY ticks,
interrupt periods and sample progression remain fixed. Remaining instruction
time survives batch boundaries; bus transactions within an instruction are
approximated as occurring when the instruction executes.

Local core changes remove stdio/debug dependencies, count unsupported-op
diagnostics, and correct IM0 acknowledge overhead to two clocks in addition to
the executed opcode. An isolated RST timing check and real-ROM partitioned-run
comparison cover these changes. The old cycle core was removed during source-distribution cleanup; its provenance
is recorded in vendor/README.md.

V3's console run completed 82.15 emulated seconds in 82.31 elapsed seconds,
with no CPU/vector errors or audio starvation, and the user confirmed correct
speed. Simulation accounted for 39.99 seconds of OS timing; drawing/submission
only 0.49 seconds (0.60%). See [HISTORY.md](HISTORY.md) and the archived logs.

## Native graphics and vector decoding

The panel is **480×272**, independently queried and tested; the seller's
resolution was incorrect. Software output uses RGB565: red bits 11–15, green
5–10, blue 0–4. The original square coordinates map to 272×272, centered at
x=104. Cohen–Sutherland clipping retains black side margins; overlapping lines
retain the brighter intensity.

V5 replaces pixel-rounded Bresenham lines with a major-axis integer DDA. Mapped
endpoints keep eight fractional bits; quotient/remainder stepping retains the
minor coordinate without cumulative slope truncation. Each step blends two
neighboring pixels using complementary quarter weights, retaining four nonzero
coverage levels (¼, ½, ¾, full) plus zero. A small table combines coverage with
the ROM's 16 brightness levels. Very short vectors still mark their midpoint.
Edge samples are clipped to the square playfield, including adjacent pixels.
No floating point, 64-bit division, larger framebuffer or downscaling pass is
needed. Original brightness and max blending are retained; smoothing can make
an individual pixel dimmer by distributing its intensity over two pixels.

The [host comparison](../logs/omega-race/v5-render-benchmark.txt) replayed six
actual-ROM captures 100000 times each, measuring roughly 1.1–2.1× the original
renderer cost including clears. This is a host comparison, not measured native
overhead. V3's 0.60% drawing/submission cost suggests sufficient headroom; the
final console test must confirm visual quality and continued full-speed sound.

The DVG supports absolute moves, long/short vectors, scale, halt, jump,
subroutine and return, with bounded instruction count, stack and line capacity.
Blank vectors still move the beam. Rate-multiplier rounding follows the
[MAME DVG reference](https://github.com/mamedev/mame/blob/master/src/devices/video/avgdvg.cpp).
The 256-byte PROM is validated but not executed as a circuit state machine.

`sysprobe/fb_video.c` owns the verified five-buffer pool and driver-address
mapping. It excludes current/recent scanout buffers, maintains black borders,
and retains the pool if stopping video fails. Never free an allocation the
driver may still be scanning. Shared ABI details are in
[FRAMEBUFFER.md](../sysprobe/FRAMEBUFFER.md).

`GPU.KO` strings show a command-buffer driver, but the inspected game/GUI
libraries did not expose recognizable OpenVG entry names. This neither proves
nor rules out hardware support. The user-supplied [OpenVG.md](../OpenVG.md) is an
unverified SDK/API research lead, not recovered firmware documentation.
GPU investigation is deferred: accelerating the measured 0.6% drawing cost
offers little benefit. The separate renderer allows later anti-aliasing/glow
or a hardware backend without changing the board model.

## Audio and clock master

Two integer AY synthesizers supply six tone/noise/envelope channels. AY timing
uses the original chip clocks; the output is mixed at 44100 Hz with a DC blocker
and signed 16-bit-scale values stored in 32-bit words. Mono output is duplicated
into two native API planes for the console's single speaker. The verified
format is `{2,44100,0}`, sector parameter 2048 and sector count 8.

`libsound.so` precision 0 expects signed 16-bit-scale values in 32-bit storage.
It can modify submitted sample storage, so each block is refilled before reuse.
The queue query reports **frames**, including partial-sector data, not raw bytes.
These details were recovered during DOOM; see [AUDIO.md](../sysprobe/AUDIO.md).

DAC sample consumption is the emulation clock master. The target is 9216 frames
including samples in the software ring, roughly 209 ms; refilling stops above
8192 native queued frames. Each block has 1024 frames. Wall-clock-only scheduling
can drift from the DAC and eventually empty the queue. No CPU time is dropped
to catch up. Callback work is bounded by eight 15000-clock chunks and about three
OS ticks, pumping audio between chunks. The tested v3 maximum callback was 20 ms.

A one-second GUI timer calibrates the 100 Hz OS tick counter, followed by 0.2
seconds of emulation priming with scheduler yields. Fractional scheduling targets
40 presentation updates/sec despite the 10 ms OS granularity; v3 submitted
38.5 frames/sec. This is independent of the original board's interrupt rate.

## Input and lifecycle

The GUI message service filters combinations even when key remapping is
disabled. Both games open `/dev/key` read/write, select ioctl `0x4003` mode 1,
and read complete 12-byte held-state records. Only built-in source 0 is used.
The service's four-byte reads cannot consume mode-1 records. Raw input and the
temporary `sys_forbid_key_mapping(1)` setting are restored on normal/error exit.
Start/Select receive a 50 ms stability filter for ADC transients; turn/fire/
thrust remain immediate. [INPUT.md](../doom_x7/INPUT.md) records the disassembly
and physical-button evidence.

Startup follows the shared CRT/applib process lifecycle, validates ROMs with
known size and bounded reads, restores battery RAM, then initializes GUI,
framebuffer, keys and sound. Reads do not assume POSIX EOF behavior. GUI exit is
called only after dispatcher registration; the initial missing-ROM error exposed
an earlier unsafe call before `gui.so` was loaded. Cleanup kills timers,
unregisters dispatch, stops sound, saves changed RAM on normal exit, restores
keys, stops video and releases native libraries/lifecycle state.

## Battery-RAM file and logging

`nvram.c` stores the entire 256-byte region at the internal-disk path in
`paths.h`. `OMEGA_RESOURCE_DIR` defaults to `/mnt/diska/res/omega`, with all eight
ROM files and the NVRAM file directly in that directory. This is independent of
the installed app's name and current working directory. All multibyte fields
are encoded explicitly, independent of C layout.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | `X7ORNV1` plus NUL: magic and format version |
| 8 | 4 | Little-endian payload length, 256 |
| 12 | 4 | Little-endian standard CRC-32 of payload |
| 16 | 256 | Original board RAM `5c00–5cff` |

Total file size is exactly 272 bytes. Loading uses a temporary buffer and
commits to emulated RAM only after size/header/CRC/close checks. Missing or bad
data never partly changes the board RAM. Saves loop over short writes, sync,
close and rename a temporary file; the old final file is never truncated.
Only RAM changed since startup is saved, after a normal exit with playback
stopped. Storage faults are nonfatal and reported through failure-only logging.
No periodic checkpoints are performed: normal quit is required for persistence.

`X7_OMEGA_DEBUG_LOG` defaults OFF. The release does not open or truncate the
routine SD log. Failures lazily open a separate error log; CRT failures retain
their independent early-error file. Existing logs are preserved by successful
release runs. The counters remain available for diagnostic builds; no per-frame
file I/O is performed in either configuration.

## Remaining approximations

The watchdog is counted but expiry does not reset the board. DVG busy duration
and rare counter-wrap clipping are approximate. AY analog filtering/cabinet
response, vector phosphor persistence, artwork, cocktail mode and two-player
controls are not modeled. There is no settings/pause UI. Host tests do not
establish physical latency or power-loss durability. V4's final hardware test
should verify a real high score surviving normal quit and restart. V5 also needs
native confirmation of antialiased detail and performance.
