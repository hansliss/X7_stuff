# Building and validating Omega Race

Run these commands from the workspace root. Keep `cmake/`, `tools/`, `syscalls.c`,
`syscalls.h`, `SYSCFG.SYS`, `binaries/`, `sysprobe/` and `doom_x7/input_stubs.S`
alongside `omega_x7/`; this is a standalone target within the shared workspace,
not a self-contained source export.

## Dependencies and release build

Requires CMake 3.20+, Python 3, and GNU `mips-linux-gnu-gcc`, `objdump` and
`readelf`. Host tests additionally use a native C compiler with ASan/UBSan and
the eight supplied ROMs in the workspace's `omega_race/` directory. There are
no extra downloaded dependencies or MAME runtime dependencies.

```sh
cmake -S omega_x7 -B /tmp/x7-omega-release -DX7_OMEGA_DEBUG_LOG=OFF
cmake --build /tmp/x7-omega-release --target omega-race -j4
python3 omega_x7/tests/run.py
```

Install `/tmp/x7-omega-release/omega-race.app` as the calculator replacement.
Its companion `.app.dis` and `.app.elfstruct` files contain disassembly and ELF
inspection results. No prebuilt `.app` or inspection report is shipped in this source directory;
build outputs stay in the build tree. Installation
and the final hardware test are manual; builds do not deploy to the console.

The top-level CMake project also includes target `omega-race`; its output is under
the build's `omega_x7/` directory. Override the tool prefix with
`-DX7_TOOLCHAIN_PREFIX=...` if necessary. The default load base is `0x69800000`
(`APP_BASE`); changing it requires checking the actual firmware address space.

Compilation is little-endian MIPS O32, `-march=24kec -O2`, freestanding, without
PIC or the host C runtime. The shared linker script supplies two LOAD segments
and zero-initialized BSS; the verified CRT creates the native application thread.
The app uses firmware syscall stubs rather than Linux syscalls.

## Diagnostic build

```sh
cmake -S omega_x7 -B /tmp/x7-omega-debug -DX7_OMEGA_DEBUG_LOG=ON
cmake --build /tmp/x7-omega-debug --target omega-race -j4
```

This writes `/mnt/card/omega-x7.txt`, with startup markers, ROM validation,
calibration, CPU/vector counters, audio queue histogram, timing and exit status.
Startup markers are synced; no per-frame filesystem output is performed.
The release build retains failure-only logs, described in [README.md](README.md).
CMake options are cached: specify ON/OFF explicitly when reusing a build folder.

Log values are hexadecimal. Divide main CPU cycles by 3,000,000 for emulated
seconds and elapsed OS ticks by the calibrated ticks/second for elapsed time.
Allow for buffered/primed audio and cleanup at the interval boundaries. Frames
count successful submissions, not independently measured LCD refreshes. OS
timing counters have 10 ms granularity on the tested firmware; some measured
categories overlap, and they should not be summed as a total CPU utilization.

## Checks and their limits

The MIPS build validates all 243 shared syscall stubs, 24 UI stubs, the
key-mapping stub against `emulator.app`, loader entry, defined symbols, LOAD
segments and supplied firmware address ranges.

`tests/run.py` uses temporary files under `/tmp` and tests:

- All eight ROM CRCs and a scripted 30-second ROM boot/gameplay/sound session.
- ASan/UBSan execution of the real ROMs and isolated CPU/DVG/render checks.
- Identical machine state and PCM when execution is divided into small batches.
- Gray-code spinner encoding, ROM/RAM behavior, IM0 timing, malformed vector
  lists, subroutines, clipping and framebuffer guards; antialias quarter weights,
  all line octants, endpoint reversal, transposition, collapsed vectors and
  intensity/overlap ordering.
- Native PCM scale/queue feeding, combined input, Home exit, and resource cleanup
  after ROM/GUI/framebuffer/audio/timer failures, including guarded pre-GUI exit.
- Battery RAM round-trip, partial I/O, corrupted/truncated/oversized saves,
  write/sync/close/rename failures preserving the previous committed save,
  a second process restoring ROM-generated RAM, and quiet/error-only builds.

The runner prints its artifact directory with previews and diagnostic logs.
Mocks validate API use and lifetime rules, not physical sound latency, native
storage power-loss behavior or console throughput. V3 has full-speed hardware
evidence; v4's save/restart behavior and v5's antialiasing still need the final
console check.

## Rendering comparison benchmark

```sh
python3 omega_x7/tests/benchmark_render.py
```

This boots the actual ROMs with scripted input, captures six scenes, then
replays each 100000 times with v5 and the preserved v4 renderer. CPU emulation
and audio are excluded from the renderer timing; both paths include framebuffer
clears. Use `--frames 10000` for a shorter comparison. The printed temporary
directory contains vector lists and before/after PPM images. The preserved
`tests/render_baseline.c` is linked only by this benchmark, not the app.
Host clock/cache/compiler differences mean ratios are guidance rather than an
X7 speed measurement. The retained [comparison](antialias-comparison.png) shows
v4 at left and v5 at right on identical captured scenes.

## Source-distribution cleanup

The sole native target is `omega-race`; host tests and the optional renderer
benchmark are separate scripts, not app targets. Earlier binaries, generated
inspection reports, cache files and unused CPU/reference headers were removed.
The active CPU MIT license and adapted AY zlib notice are retained in `vendor/`.
Shared firmware files are required as listed above; this folder alone is not
a self-contained SDK. No ROM files are embedded in the source package or app.
