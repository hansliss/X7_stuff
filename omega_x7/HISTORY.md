# Omega Race development and console measurements

Current checkout: source-only; rebuildable binaries are not bundled. The games
now have independent launcher entries and console-confirmed resource loading
from `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/`; Calculator is restored.
Earlier binary/path/test directions below are historical. Current instructions
are in the game READMEs and root SESSION.md.

Earlier binaries and generated reports were removed during source cleanup.
Statements below about retained binaries describe the historical state; version
hashes and raw console evidence remain. Historical test directions are evidence. Use
[README.md](README.md) for the current app and [BUILD.md](BUILD.md) to rebuild.

v1 app SHA-256: `d299dcbbca6cf6c9e4280eb271dfa379410f80a50c4e77be9a8d48fb1ab6fa99`.

## v2 startup correction

The first hardware log stopped at the initial `omega.m7` load before graphics
or audio setup. The precise file operation is not established by that log.
An early failure could call `ui_exit_loop` before `gui.so` was available;
v2 calls it only after the GUI dispatcher is registered. File-loading errors
are synced immediately and follow the cleanup path.

ROM sizes are checked with the firmware `sys_fstat` layout validated in DOOM.
Reads are bounded to 1024 bytes and yield between blocks; the extra EOF probe
is removed. Synced markers distinguish full file path, open result, stat,
read, close and CRC. The key driver is opened read/write as in the working
DOOM app. The v1 binary remains available as `omega-x7-v1.app`.

Host tests now reject pre-GUI exit calls and cover missing ROMs, wrong file
sizes, read errors, checksum errors and a non-POSIX EOF error return. Full
host checks and the MIPS loader/syscall checks passed. The v3 console test subsequently verified these retained startup changes.

v2 app SHA-256: `8fc8bbf8321a30291e00473fa35a725013e37f0c28b0f04b277eb07be55f3dd0`.

## v3 performance change

The successful v1 log is archived at `logs/omega-race/v1-play.txt`. It completed
109,927,500 main-CPU clocks (36.6425 emulated seconds) over 146.47 wall seconds:
25.0% of intended speed. Simulation consumed 73.14 seconds of reported OS time,
while rendering/submission consumed only 0.28 seconds (0.19% of the session).
The board and sound paths worked without vector errors or audio-ring overflow.
Drawing acceleration would not address the main cost in that run.

V3 replaces per-clock Z80 bus emulation with an instruction-based core, and
schedules the next CPU/peripheral event instead of traversing every clock.
Remaining instruction time survives batch boundaries; both CPU rates, AY
clocks and interrupt periods remain the same. Individual bus edges within an
instruction are approximated. Full-ROM boot/gameplay and partitioned-run tests
pass without CPU diagnostics or vector errors.

Callback batches can now run for about three OS ticks instead of one, with at
most eight 15000-clock chunks. This reduces scheduling gaps while preserving
bounded input/system-message service. Audio-queue samples, pump time, callback
count/duration and CPU diagnostics are added to the exit log.

Startup fixes from v2 are retained. The missing subdirectory caused the initial
hardware failure; the v1 error path could turn that missing-file error into a
hang. V2's guarded cleanup and checked-size reads still improve failure handling.

The successful v3 console log is archived at
[`v3-play.txt`](../logs/omega-race/v3-play.txt). The user confirmed correct speed,
controls and sound. It completed 246,448,758 main-CPU clocks (82.149586 emulated
seconds) over 82.31 wall seconds, approximately 99.8% of real time. The short
difference includes cleanup and buffered audio; this is consistent with full
speed. It presented 3169 frames (38.5 per second), with callbacks taking at most
20 ms. No CPU diagnostics, vector errors, vector overflows, empty playback
queue queries or audio-ring overflows were recorded, and exit status was zero.

Simulation consumed 39.99 seconds of reported OS time, audio pumping 2.95
seconds, and drawing/submission just 0.49 seconds (0.60% of the session).
OpenVG investigation is deferred because drawing is already inexpensive.
The user's [`OpenVG.md`](../OpenVG.md) is retained as a research lead; its
proposed SDK/API integration has not been verified against this firmware.
High scores did not persist in v3; v4 adds persistence. Control assignments remain unchanged.

On the same host, the same 30-second single-machine ROM script took 1.888 CPU
seconds with the v1/v2 cycle model and 0.475 seconds with v3, about 4.0× faster.
This is a host comparison, not a console speed prediction.

v3 app SHA-256: `bb5fb60a6f09024611aea1b60aed36de0e49e15afe01a6f17b04ea5f2719ebb7`.

## v4 final persistence and quiet build

V4 leaves the console-tested CPU, vector, input and audio implementation intact.
It restores the board's entire 256-byte battery RAM before CPU execution and
saves changed RAM on normal exit. The 272-byte file has a versioned header,
explicit length and payload CRC; replacement uses a synced temporary file and
rename. Missing data uses fresh board RAM; corrupt/read-failed data never
partly overwrites it. Failed write/sync/close/rename retains the old final file.
No filesystem save runs in the gameplay callback.

`X7_OMEGA_DEBUG_LOG` defaults OFF. Successful runs do not open or truncate the
old routine log. Failures retain separate diagnostic files and do not require
a writable SD card to exit safely. Native cleanup still evaluates all restoring
calls even when their diagnostic output is disabled.

Validation passed on 2026-10-11: real-ROM gameplay and batch-partition comparisons,
ASan/UBSan, DVG/render/input units, persistence round-trip and I/O fault checks,
second-process restoration, corrupt-save fallback, quiet/stale-log preservation,
and existing platform failure/cleanup cases. The MIPS release build passed shared
syscall, entry, segment, UI and key-mapping checks. The final native high-score
quit/restart test remains pending.

Retained reports: [host checks](../logs/omega-race/v4-host-checks.txt),
[release build](../logs/omega-race/v4-release-build.txt), and
[debug build](../logs/omega-race/v4-debug-build.txt). Both native configurations
passed their loader/stub checks.

The console-tested v3 app is retained as `omega-x7-v3.app`. The current
ready binary is v4 with routine logging disabled.

v4 release SHA-256: `746f1feb0a7139fddd79b187b93a50a7a53eff66be5e6ba3bd93eba34c872ad5`.

## v5 four-level antialiasing

The user requested smoother fine details after confirming there was likely
enough drawing headroom. V5 keeps endpoint coordinates fractional and replaces
pixel-snapped lines with an integer quotient/remainder DDA. Each step distributes
brightness across adjacent pixels with quarter coverage. It retains the ROM's
brightness levels, max overlap blending, full-height square and black margins.
CPU emulation, audio, input, persistence and the quiet default are unchanged.

Six captured ROM scenes, replayed 100000 times each, took about 1.1–2.1× the
old renderer's host time including clears. This is not a MIPS prediction; the
old native drawing/submission cost was just 0.60% of elapsed time. The native
test should check small circles/text/ships, normal speed and continuous sound.

Coverage tests include exact quarter-weight pixels, octants, reversal,
transposition, short/collapsed vectors, intensity/overlap order, clipping and
guards. These and the full real-ROM/sanitizer/audio/persistence/cleanup suite
passed. Release and debug MIPS builds passed entry/layout/stub checks.

Reports: [host checks](../logs/omega-race/v5-host-checks.txt),
[release build](../logs/omega-race/v5-release-build.txt),
[debug build](../logs/omega-race/v5-debug-build.txt), and
[renderer comparison](../logs/omega-race/v5-render-benchmark.txt).
The [before/after image](antialias-comparison.png) shows v4 at left and v5 at right.
The quiet v4 app is retained as `omega-x7-v4.app` for comparison.

v5 release SHA-256: `b8c12366482bc7de0681fdebc93db2907d6a7203c03c6d99df5a32b35dab3fa5`.

## Resource-path update

After the relative-path experiment failed in DOOM, both games moved resources
away from the calculator app directory. `paths.h` now defines the sole Omega
base, `/mnt/diska/res/omega`; the eight ROMs and `omega-race.nvram` are directly
inside it. No emulator/audio/rendering/input behavior changed. Manual file
migration is required. Host prefix checks and both native apps passed.
Current installation instructions are in README.md; historical paths above
describe the original test builds.

Current resource-path build SHA-256:
`0c94d595b44654d8f81346400dda13251d2a9f882a96ec3c81b2da71ff60404b`.
