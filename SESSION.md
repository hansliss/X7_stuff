# X7 game ports and lasting session findings

This is the current index and decision record for the DOOM and Omega Race work,
plus the audio/input/timeout discoveries made along the way. Project READMEs
contain current installation instructions. Historical experiment directions and
superseded interpretations are retained in their original documents as evidence.

## Current applications

| Project | Build output | Status and instructions |
| --- | --- | --- |
| DOOM | `doom.app`, v13 | Completed port; [PLAY.md](doom_x7/PLAY.md) |
| Omega Race | `omega-race.app`, v5 | V3 full speed; current own launcher entry and `res/omega` loading confirmed; [README.md](omega_x7/README.md) |

Both are freestanding native `.app` files installed with their own launcher
entries. The user confirmed that layout and restored Calculator. They share
the verified firmware CRT, syscall stubs, GUI lifecycle and framebuffer helper.
The calculator replacement route was used for the earlier development tests. Builds do not install or patch
the console. Keep external WAD/ROM files unchanged.

## Hardware and interfaces worth reusing

- **Panel:** 480×272, RGB565 with red in bits 11–15, green 5–10 and blue 0–4.
  This was established by the driver query and console tests; seller resolution
  information was wrong. See [FRAMEBUFFER.md](sysprobe/FRAMEBUFFER.md).
- **CPU/ABI:** little-endian MIPS O32, freestanding application thread and
  fixed vendor library/syscall tables. The shared build uses `-march=24kec`.
  `.app` is a firmware-loaded ELF, not a Linux executable. See
  [syscalls.md](syscalls.md) and [GUI.md](sysprobe/GUI.md).
- **Scheduler:** the tested OS tick rate is 100 Hz. GUI timers and bounded work
  callbacks keep system tasks responsive. Long blocking load/log operations
  need yields; a nominal 1 ms timer does not imply 1 ms native scheduling.
- **Video buffers:** use the verified allocation/address translation and
  current/recent scanout exclusion. Do not free the pool when stopping video
  fails; the driver may retain it. Preserve the shared helper's cleanup guards.
- **Speaker:** one physical speaker. Native audio nevertheless uses two planes
  with the same mono sample pointer in the tested setup.
- **Storage:** `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/` hold each game's
  assets and persistent files, independent of the launcher replacement;
  `/mnt/card/` holds optional diagnostics. Load files using the recovered
  `sys_fstat` structure and bounded reads, without relying on a POSIX EOF probe.
- **OpenVG:** a firmware GPU command-buffer driver exists, but a callable
  OpenVG/EGL integration has not been recovered. [OpenVG.md](OpenVG.md) is the
  user's saved AI-generated research lead, not proof of this firmware API.
  Omega Race drawing already costs about 0.6% of elapsed time, so investigation
  is deferred. Neither game needs hardware vector acceleration to run well.

## DOOM result and decisions

The port uses the cloned original Linux DOOM 1.10 source and the user's 1.9
registered WAD. Asset/map loading works, and the original timedemo adaptation accepted the
WAD's version-109 recordings rather than the engine's initial version refusal.
The first silent timedemo achieved roughly 88 submitted frames/sec, establishing
headroom beyond the normal 35 game tics/sec. See
[DOOM-X7-FEASIBILITY.md](DOOM-X7-FEASIBILITY.md) and
[doom_x7/README.md](doom_x7/README.md) for source adaptations and measurements.

The final app renders at 320×200 and converts to centered 363×272 output,
preserving DOOM's intended 4:3 CRT appearance on the actual panel. Pixel doubling
was dropped after verifying panel dimensions. The title waits for a button;
the first press opens the main menu and is consumed. Startup menus have a black
background to improve readability. Startup measured **3.6 seconds** after
reducing calibration to one second, batching loading yields and buffering logs.
The earlier roughly 20-second startup was substantially explicit delay/logging
overhead; moving the WAD alone was not the complete fix.

Console tests confirmed controls and combinations, menu behavior, weapon
cycling, save/load, level transitions, respawn, music tracks, independent music
and SFX volume, saved settings and scaling. Monster retaliation/infighting
comes from the original engine logic (`p_inter.c`), not new port behavior.
An occasional reported hiss was not reproduced in the later test and was
deferred; no distinct hiss cause is claimed.

Music interprets WAD MUS data using the original GENMIDI bank and an integer
OPL2 synthesizer. The first music implementation was too costly and quiet;
the RP2040-derived emu8950 implementation and revised gain restored responsive
gameplay and suitable volume. The hardware test is the performance evidence;
the roughly 4.3× host comparison guided selection but did not predict MIPS speed.
Licenses and adaptations are in [doom_x7/vendor/README.md](doom_x7/vendor/README.md).

WAD: `/mnt/diska/res/doom/doom.wad`; settings: `doom.cfg`; six save slots:
`doomsav0.dsg` through `doomsav5.dsg` in the same directory. Quit normally to
persist settings. Gameplay/menu mappings live in
[x7_controls.h](doom_x7/include/x7_controls.h). Routine logs are off by default;
`X7_DOOM_DEBUG_LOG=ON` restores diagnostics. The final refactor kept the
hardware-tested v12 assignments/filtering; see PLAY.md for validation boundaries.

## Audio corrections established during DOOM

The stock emulator and `libsound.so` supplied the working native API. The final
verified format is `{2,44100,0}`, with 1024-frame blocks, sector parameter 2048
and sector count 8. **Precision 0 takes signed 16-bit-scale samples stored in
32-bit words.** The library's final converter shifts/clamps them; the earlier
24-bit-scale interpretation and ×256 scaling caused saturation and are
superseded. Submitted buffers can be modified by the library, so regenerate
them before reuse. See [sysprobe/AUDIO.md](sysprobe/AUDIO.md) for converter
addresses and the reusable API.

The initial queue cutoff stopped feeding while partial sectors were retained.
Removing saturation fixed distortion; a deeper roughly 186–209 ms queue with
frequent refills fixed fragmented playback. Queue queries report frames,
including partial-sector counts. The recording supported investigating
starvation but could not prove an exact periodic gap rate. DOOM's original quit
sound waits were also changed to keep pumping during the wait. Current hardware
tests reported smooth effects/music. See [doom_x7/AUDIO.md](doom_x7/AUDIO.md)
for the complete correction history rather than its superseded early scale.

Omega Race reuses this PCM interface, while the DAC's actual consumption drives
both emulated CPUs and AY sample generation. Wall-clock-only feeding can drift
against the audio device. Save/log writes stay outside real-time callbacks.

## Input discoveries and controls

The firmware initially remapped left/right masks `0x40/0x80` to shoulder masks
`0x100/0x200`, making both pairs strafe. `sys_forbid_key_mapping(1)`, the stock
emulator's API, removes this remapping temporarily. It is restored on exit.
This alone did not solve combinations: the GUI service's event filter locks
onto the first accepted button and suppresses other simultaneous states.

Both games therefore use `/dev/key` ioctl `0x4003`, mode 1, and complete 12-byte
held-state records. The service's four-byte reads cannot dequeue these records.
Snapshot comparison preserves movement while adding/removing fire or shoulders;
only built-in controller source 0 is accepted. Cleanup restores mode 0, closes
the driver and restores firmware mapping. The user verified both press orders
and several three-button combinations; exhaustive permutations are not claimed.

Left stick/D-pad and right stick/face-button aliases remain firmware behavior.
Start/Select and shoulders share an ADC ladder. Unexpected menu/map openings
following shoulder releases were eliminated by requiring 50 ms stability only
for Start/Select presses; gameplay/release handling stays immediate. See
[doom_x7/INPUT.md](doom_x7/INPUT.md) for exact disassembly and physical traces.

DOOM's final controls: directions move/turn, shoulders strafe, A fire, B use,
X run, Y next owned weapon, Select map, Start menu. Menu A/B confirm/back.
Omega Race's controls: directions turn, A fire, B/Up thrust, Select coin,
Start one player, held Home quit. Editable mappings are project-local headers.

## Backlight timeout patch

The requested `usb.app` location at `0x62c00494` reads `BACKLIGHT_DURATION`
for USB charging's backed-up timeout. It is not the normal system idle timer.
`msg_serv.app` owns that timer and skips creation/restart for duration zero.
The checked patch forces zero in configuration loading and runtime duration
updates; the USB patch sets the charging backup argument to zero and preserves
the earlier manufacturer-disk patch.

The user reported that the fix worked. The conversation did not record which
patched images were installed, so that does not establish separate coverage of
every USB/charging/runtime path. The generator only writes local copies.
Full instructions, addresses, original/replacement bytes, reversibility and
the decision against recompiling decompiled USB code are in
[dumper/backlight_patch/README.md](dumper/backlight_patch/README.md).

Use `python3 dumper/backlight_patch/patch.py` to regenerate checked patched
copies. Do not infer file offsets from virtual addresses: use ELF LOAD mappings.
MIPS call delay slots can change the argument immediately before a `jalr`
callee executes, allowing a small instruction patch rather than an app rewrite.
The patch enforces effective timeout zero; it does not repair configuration
persistence or unrelated sleep/shutdown timers.

## Omega Race result and decisions

The eight unmerged ROM files exactly match the original arcade set. The app
models the 3 MHz main and 1.5 MHz sound Z80s, board RAM/I/O, periodic IRQ/NMI,
Gray-code spinner, vector generator and two AYs. It renders intensity-valued
white vectors into a centered 272×272 software framebuffer. The ROM runs its
own gameplay and score logic. Cabinet art, phosphor glow and analog audio
filtering are omitted; timing is an instruction-level approximation.

The initial missing ROM subdirectory caused load failure. An unsafe pre-GUI
exit could turn that error into a console hang. Guarded cleanup and bounded
size-checked ROM reads were retained even after the corrected install booted.
The first successful run had working display, controls and sound but 25% speed.

Switching from per-clock to instruction-based Z80 execution and event scheduling
restored full speed without changing nominal CPU/chip clocks. The successful
v3 log shows **82.15 emulated seconds / 82.31 elapsed seconds**, 38.5 submissions
per second, maximum callbacks of 20 ms, no CPU/vector errors or audio queue
empties/overflow, and clean exit. Simulation took 39.99 seconds of reported OS
time; drawing/submission 0.49 seconds. GPU work would not fix the former CPU
cost. Raw evidence and version hashes are in
[omega_x7/HISTORY.md](omega_x7/HISTORY.md).

V4 preserves all 256 bytes of battery-backed RAM, with size/version/CRC checks
and synced temporary-file replacement on changed RAM at normal exit. No storage
work runs during gameplay. The default build is quiet; failures still have
optional SD diagnostics. High-score restart and quiet/error logging passed
host tests; a final native high-score test remains pending. See
[TECHNICAL.md](omega_x7/TECHNICAL.md) for the file format and limitations.

V5 adds four nonzero edge-coverage levels using a fractional-coordinate integer
line renderer. This improves small outlines without a larger framebuffer,
floating-point math or GPU dependency, preserving original vector brightness.
Six actual-ROM host scenes took roughly 1.1–2.1× the old renderer time; this is
not an X7 measurement. V3's measured drawing cost was only 0.6% of the session.
Antialias coverage, octants, reversals, bounds and the existing integration tests
passed, as did both native builds. Before/after images, benchmark instructions
are retained; earlier comparison binaries were removed during source cleanup. Native appearance/performance remains
part of the final console test.

## Rebuilding and validation

Requires CMake 3.20+, Python 3 and the `mips-linux-gnu` GNU toolchain. Host tests
also need native `cc` with ASan/UBSan. Shared stub/layout checks use the original
firmware reference files in this workspace. Keep the cloned DOOM source and
the user WAD/ROMs for real-asset tests.

```sh
cmake -S doom_x7 -B /tmp/x7-doom-release -DX7_DOOM_DEBUG_LOG=OFF
cmake --build /tmp/x7-doom-release --target doom -j4
python3 doom_x7/tests/run_host.py --play
python3 doom_x7/tests/run_music.py

cmake -S omega_x7 -B /tmp/x7-omega-release -DX7_OMEGA_DEBUG_LOG=OFF
cmake --build /tmp/x7-omega-release --target omega-race -j4
python3 omega_x7/tests/run.py
```

The resulting `.app` files are in the corresponding build directories. Build
options are cached; specify debug logging explicitly when reusing a build tree.
Original probe sources remain available; versioned game binaries and generated reports
were removed from the cleaned game directories. Each now ships only
production sources, tests, license notices and documentation; canonical apps
are generated in the build directory.
Tests cover actual game assets, sanitizers, input combinations, rendering guards,
audio feeding, persistence fault cases, lifecycle cleanup and firmware stubs.
They do not substitute for physical display/audio/input/storage behavior. Keep
the distinction between host comparisons and console measurements in future work.

## Launcher-independent resource directories

The relative-path experiment failed on the console, so game files now use
absolute paths independent of the calculator replacement location. One header
per game defines the base directory: `doom_x7/include/x7_paths.h` defaults to
`/mnt/diska/res/doom`, and `omega_x7/paths.h` to `/mnt/diska/res/omega`.
DOOM's WAD, config, temporary config and all save-slot references (including
generated engine paths) derive from its base. All eight Omega ROM files and
battery RAM derive from its base; ROMs go directly in that directory.
Changing the header and rebuilding is sufficient; `prepare.py` no longer
contains literal resource directories. Diagnostic/native library paths stay
separate. Existing files must be moved manually; the app does not create the
directories or migrate data. Host suites enforce the resource prefixes, and
both canonical apps passed native loader/stub checks after the change.

## Independent launcher entries and source-only check-in

The user confirmed both games launch through their own entries while Calculator
is restored, and resources load successfully from `res/doom` and `res/omega`.
Those directories decouple game data from the app each game originally replaced.
The launcher/resource transition is native evidence; a separate high-score
restart and antialias appearance/performance report was not recorded.

All current CMake targets were rebuilt before binary cleanup, including every
sysprobe discovery/render/audio variant and the libg1 probe variants. Rebuildable
outputs were removed from the source packages and local build trees. No old probe
binary needed an exception. Original firmware/dumper inputs remain because they
are required for ABI checks or patch generation. Generated backlight patch apps
are removed and ignored; the checked script recreates them. Source-only ignore
rules also cover game/probe outputs, external engine/ROM inputs and returned
experiment files. No commit or console installation was made by this cleanup.
