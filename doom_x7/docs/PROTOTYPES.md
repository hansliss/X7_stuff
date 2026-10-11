# Historical DOOM prototype notes

Current checkout: source-only; rebuildable binaries are not bundled. The games
now have independent launcher entries and console-confirmed resource loading
from `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/`; Calculator is restored.
Earlier binary/path/test directions below are historical. Current instructions
are in the game READMEs and root SESSION.md.

The demo targets, old binaries, build wrapper and obsolete renderer sources
were removed during source-distribution cleanup. Commands and filenames below
are historical evidence, not supported build/install instructions. Use
[../README.md](../README.md) and [../PLAY.md](../PLAY.md) for the current app.

# DOOM for the X7

**[Playable DOOM](../PLAY.md): `doom-x7-play.app`** is the final
v13 build. Console tests confirmed gameplay, full-height 4:3 display, mono sound
effects and music, saves/loads, persistent settings and 3.6-second startup.
Routine debug output is disabled. Button assignments are kept in
[include/x7_controls.h](../include/x7_controls.h) for easy changes and rebuilding.
See PLAY.md for installation and build instructions.

The remaining sections describe the retained development probes.

The new **[normal-speed demo with sound effects](../AUDIO.md)** is available as
**`doom-x7-audio.app`**. It targets 35 simulation tics/second
and logs to `doom-x7-audio.txt`. V5 increases audio buffering and compares 1/2, 1/3 and
1/4 audio gain, labeled above the game. The silent throughput benchmark is described
below.

Ready-to-copy binary: **`doom-x7.app`**. This is a freestanding
little-endian MIPS O32 `.app`, using the verified application startup and
shutdown sequence. The first console run completed DEMO1 successfully at
approximately **88 rendered/submitted frames per second**; see the measurements
below. It automatically plays an unpaced timedemo to measure engine
throughput; this first binary has no interactive gameplay controls or audio.

## Install and run

1. The retained silent binary uses **`/mnt/card/DOOM.WAD`** (SD card root, uppercase).
   The current audio binary and newly built targets use
   **`/mnt/diska/apps/calculat/doom.wad`**.
   Keep the original WAD unchanged.
2. Install `doom-x7.app` as **`calculat.app`** using the same calculator
   replacement and tools-menu launch procedure that worked for the demos.
3. Launch it. Initialization and a five-second timer calibration show a black
   screen. Then the built-in **DEMO1** plays, possibly faster or slower than
   normal gameplay. It exits automatically when the recording ends.
   **START/DOWN cancels** at any point serviced by the message loop.
4. Return **`/mnt/card/doom-x7-benchmark.txt`**, and report whether the image
   was correct and the tools menu worked afterward. If it fails before the
   worker starts, look for **`/mnt/card/doom-x7-start-error.txt`** too.

Each run overwrites its benchmark log. The separate CRT error file may be
left over from an earlier run. Configuration, saves, and WAD contents are not
written. Allow initial loading to finish before interpreting a black screen
as a failure; the last synced marker identifies setup progress.

The output is exactly **320×200**, centered at **(80,36)** in the existing
console-verified **480×272** mode. There is no scaling, pixel doubling, crop,
or aspect correction. Both framebuffer code and the console's earlier query
established those panel dimensions independently of the seller. This binary
requires that tested mode and stops if the framebuffer query disagrees.

## Optional comparison runs

Create **`doom-x7-mode.txt`** in the SD card root containing two ASCII digits:
the first selects output work, the second selects DEMO1, DEMO2 or DEMO3.
Missing or invalid settings fall back to `11`.

| File contents | Run |
| --- | --- |
| `11` | DEMO1, render and submit (default) |
| `21` | DEMO1, render but omit conversion and framebuffer submission |
| `31` | DEMO1, simulation only, omit rendering and submission |
| `12` / `13` | Full output for DEMO2 / DEMO3 |

Comparison modes still initialize the display and stay black. Use the same
demo across all three modes when comparing costs. All modes retain message
service and scheduling between bounded batches.

The supplied recordings contain **1710**, **2347**, and **3863** single-player
simulation tics. A successful complete run should report those counts.
Version **109** recordings are accepted by the 1.10 engine. Map/asset loading
was already compatible; the change bypasses the original demo-version refusal.

## Reading the log

Values are hexadecimal. `exit reason` 1 means complete, 2 means Start cancel,
3 means system quit, and 4 means an error (including failed video cleanup).
The measured interval starts after initial map loading and ends at the demo
end marker. It includes normal callback/pacing overhead and on-demand resource
loads. Initial startup and shutdown/card logging are excluded.

```sh
python3 doom_x7/analyze_log.py /path/to/doom-x7-benchmark.txt
```

The log separates simulation, display, palette conversion, buffer acquisition
and submission ticks, plus rendered and submitted frame counts. Display ticks
include the three output stages, so their percentages overlap. Per-stage tick
resolution can round short operations to zero. Callback maximums include the
first level-loading callback; the initial simulation cost is excluded from
the accumulated measured simulation ticks.

The clock estimate uses OS ticks elapsed across a **requested** five-second
GUI timer; it is an initial calibration, not an independently verified wall
clock. The analyzer labels resulting FPS/tics-per-second estimates accordingly.
An externally measured duration for just the timedemo can be supplied as
`--seconds 23.4`. Counts measure rendering/submissions, not unique LCD frames.
The useful real-time threshold is **35 simulation tics per second**; a
timedemo's throughput also reflects one rendered frame per simulation tic.

## Implementation

- Existing X7 CRT, applib lifecycle, GUI focus/quit handling and video helper.
  A single GUI message loop covers calibration and playback; it does not
  restart `get_msg` after `exit_msg_loop`.
- Indexed 320×200 engine framebuffer, converted through DOOM's current
  gamma-adjusted palette into the native channel packing. All five native
  slots retain black borders. Current/recent-address exclusion is preserved.
- At most four tics/frames per callback, returning sooner when one OS tick
  passes. A single costly frame can exceed that interval. Loading reads yield
  during setup; measured reads retain normal resource-loading cost.
- Both high-level and platform audio work are omitted. No audio device,
  sound server, signal timer, socket, X11, Linux syscall, or Linux libc is used.
- Tracked heap allocations use recovered kmalloc/kfree wrappers. A **2 MiB
  zone** follows the source's effective defaults, and completed all three
  demos in host tests. Screens, metadata, the **1.25 MiB** native video pool,
  app code/BSS and firmware libraries need additional RAM. Console allocation
  allocation succeeded in the first console run; failures are logged and cleaned up.
- Integer FixedDiv generates 16 fractional bits with unsigned arithmetic;
  no floating-point or general 64-bit division runtime is linked.
- The 35 KB WAD directory uses a bounded 64 KB static buffer. Smaller existing
  renderer temporaries still use stack allocation; actual stack headroom is
  unmeasured. The prototype disables transition wipes.
- Build-time source adaptations fix the sprite-table NULL terminator,
  event-index double assignments, and pointer/table type assumptions. The
  cloned source remains unchanged; generated copies live in the build tree.
- Video stops before pool release. If stopping fails, retain the pool instead
  of freeing memory the driver may still read, and record an error.

## Build and validation

Uses the same installed MIPS cross compiler, CMake and Python as the probes.
With the original source under `DOOM/linuxdoom-1.10`:

```sh
cmake -S . -B build
cmake --build build --target doom-x7 --parallel
# Output: build/doom_x7/doom-x7.app

# Or build standalone and refresh the ready-to-copy binary:
sh doom_x7/build.sh
```

The build checks all 243 root syscall stubs, 24 GUI stubs, the process and
lifecycle stubs, ELF entry/segments/BSS, undefined symbols and collisions
against supplied firmware load ranges. New port code builds with warnings as
errors; original engine warnings remain visible. The app uses `0x69800000`
as its fixed load base. Renaming does not relocate it.

Host integration checks run the actual engine and WAD through all three full
demos and both comparison modes. They exercise partial I/O, opaque firmware
descriptors, missing WAD, allocation/GUI/timer/query failures, Start cancel,
normal cleanup and failed-stop pool retention. Every submitted frame is
checked for untouched black borders. A separate fixed-point test compares
200,000 random inputs against 64-bit reference arithmetic under UBSan.

```sh
python3 doom_x7/tests/run_host.py
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
  doom_x7/tests/fixed_test.c doom_x7/fixed.c -I DOOM/linuxdoom-1.10 \
  -o /tmp/doom-x7-fixed-test
/tmp/doom-x7-fixed-test
```

The integration fixture uses temporary host-width ABI copies and low-address
mappings on Linux x86-64. It checks engine/control flow and cleanup, not MIPS
execution, firmware timers, cache/DMA behavior, watchdogs or console speed.
The first console run establishes working playback and successful cleanup
calls on this firmware; broader timer, cache/DMA and watchdog contracts remain
unverified.

## First console measurement

The user reported very fast playback and supplied
[doom-x7-benchmark.txt](../../doom-x7-benchmark.txt). The console's 30-second
inactivity timeout was avoided by periodically pressing left-stick left.
The prototype ignores this input for gameplay; only START/DOWN cancels.
Handling those messages can add small, unquantified overhead to elapsed time.

DEMO1 completed all **1710 tics**, with **1710 rendered frames** and **1710
submissions**, no skipped submissions, no unavailable buffers and no address
query failures. The panel query again returned **480×272**, 16-bit depth.
Video stop, blending teardown, pool release and application shutdown returned
success, and the log finished with completion reason 1, status 0 and DONE.
The log alone does not prove the tools menu remained usable afterward.

Calibration observed **498 OS ticks** across the requested five-second timer.
The measured demo interval was **1929 ticks**, giving an estimated **19.37
seconds**, **88.29 rendered/submitted frames per second**, and **2.52 times**
the 35-tic/s real-time simulation requirement for this scene. This is engine
throughput, not a measured LCD refresh rate, and depends on the GUI timer's
wall-time accuracy. DEMO1 contains about 48.86 seconds of normal-speed action.

| Accumulator | OS ticks | Share of measured interval |
| --- | ---: | ---: |
| Simulation | 67 | 3.5% |
| Display, including conversion and submission | 921 | 47.7% |
| Palette conversion (included in display) | 88 | 4.6% |
| Buffer acquisition (included in display) | 1 | 0.05% |
| Submission (included in display) | 0 | Below accumulated tick resolution |

The remaining 941 ticks (48.8%) include message/timer/scheduler costs and
uninstrumented work; they should not be treated as pure idle time. Stage
measurement uses coarse OS tick differences. The 26-tick maximum callback
includes the initial map-loading callback and is not a steady-state frame-time
maximum.

The tracked heap peaked at **2,405,837 bytes** (2.29 MiB), excluding the native
video pool and executable/library allocations. The 2 MiB zone had **1,597,096
bytes** (1.52 MiB) free or purgeable at exit; Z_FreeMemory includes purgeable
cache blocks, so this is not necessarily unused memory.

The message `couldn't open DEMO1.lmp` is the engine's optional external-demo
lookup. It then successfully used DEMO1 embedded in the WAD.

This result supports a real-time silent interactive port at the current
resolution. Busier maps, other demos, actual controls and audio still need
their own console measurements. Normal-speed play can exceed the inactivity
timeout, so power-management integration remains a separate follow-up.

The current WAD path is `/mnt/diska/apps/calculat/doom.wad`.
