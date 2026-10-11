# Action X7 handheld console syscall probes and other RE results
*Here's example code, a couple of useful tools and lots of documentation. This was
mostly created by Codex, under my guidance. So, apologies for the AI slop, but
it was very useful during this investigation.*

## Documentation map

* [Game-port session findings](SESSION.md): DOOM and Omega Race results, shared
  audio/input discoveries, timeout patch, decisions and current build commands.
* [Omega Race](omega_x7/README.md): play/install, persistent scores, quiet logging;
  [build instructions](omega_x7/BUILD.md), [implementation](omega_x7/TECHNICAL.md)
  and [console history](omega_x7/HISTORY.md).
* [Completed DOOM](doom_x7/PLAY.md): controls, assets, saves/settings, music and
  release/debug builds.
* [Backlight timeout patch](dumper/backlight_patch/README.md): checked instruction
  changes for USB charging and the system message service.
* [Syscall ABI](syscalls.md): both SYSCFG tables, arguments, 64-bit offsets,
  filesystem structures and confidence levels.
* [GUI, fonts and buttons](sysprobe/GUI.md): current library/API reference,
  message layouts, pixel packing and console-verified font dependencies.
* [Framebuffer and video](sysprobe/FRAMEBUFFER.md): driver ioctl structures,
  initialization/shutdown, pool mapping and the working buffer-ownership guard.
* [Per-pixel orbiting-light demo](sysprobe/FRAGMENT.md): the new fragment shader,
  constant-speed light motion, validation and build/run instructions.
* [Rendering demos and measurements](sysprobe/DEMOS.md): all GUI/video demo
  generations, optimizations, observed artifacts, raw timings and limitations.
* [GUI investigation history](sysprobe/UI-notes.md): reverse-engineering
  evidence and the sequence of console experiments.
* [Filesystem console results](logs/sysprobe-results.md) and
  [watchdog findings](libg1_probe/README.md#watchdog-findings).

The original [NAND dump hook](dumper/libg1_dump/README.md) also has scheduler
yields during and between files. The full run still reset after three files;
a separate one-file-per-load variant is available to test bounded loader work.

## Probe packages

The completed [DOOM port](doom_x7/PLAY.md) builds as `doom.app`, with music,
persistent settings and configurable controls.

The [Omega Race app](omega_x7/README.md) builds as `omega-race.app`. It emulates
the supplied arcade
ROMs, including vector display and sound, and runs at full speed on the console.
V4 adds persistent high scores and disables routine logging; v5 adds four-level
antialiasing. Both games now run from their own launcher entries, with Calculator
restored. Resource loading from `/mnt/diska/res/doom/` and
`/mnt/diska/res/omega/` is console-confirmed. Separate high-score persistence and
antialias quality/performance reports have not been recorded for this final build.

The two probe packages are:

* [`libg1_probe/`](libg1_probe/README.md): the console-verified `libg1.so`
  loader probe, including filesystem, metadata, directory, and watchdog variants.
* [`sysprobe/`](sysprobe/README.md): the console-verified standalone `.app`
  probe, launched by installing it as `calculat.app`, with application lifecycle
  registration and clean exit.

Game and probe packages are source-only: rebuildable binaries, disassemblies,
ELF reports and intermediates are not shipped. All current CMake targets,
including the older sysprobe variants, rebuilt successfully before cleanup.
Console result files and their generation directories are archived under
[`logs/`](logs/). Both packages share the
root `syscalls.c`/`syscalls.h`, `tools/`, `cmake/`, `SYSCFG.SYS`, and `binaries/`
for compilation and ABI validation; retain those alongside the packages.

## Build both with CMake

Requires CMake 3.20+, Python 3, and the installed `mips-linux-gnu` toolchain:

```sh
cmake -S . -B build
cmake --build build --parallel
```

The project selects `cmake/x7-mips-toolchain.cmake` automatically on the first
configure. It builds freestanding little-endian MIPS O32 executables, with no
host libc dependencies. Outputs are:

```text
build/libg1_probe/libg1.so              # default filesystem variant
build/libg1_probe/libg1-fs.so
build/libg1_probe/libg1-metadata.so
build/libg1_probe/libg1-directory.so
build/libg1_probe/libg1-watchdog.so
build/sysprobe/sysprobe.app
build/sysprobe/sysprobe-ui.app
build/sysprobe/sysprobe-font.app
build/sysprobe/sysprobe-hsv.app
build/sysprobe/sysprobe-bitmap.app
build/sysprobe/sysprobe-dodeca.app
build/sysprobe/sysprobe-dodeca-fb.app
build/sysprobe/sysprobe-dodeca-fast.app # verified enlarged video demo
build/sysprobe/sysprobe-dodeca-fragment.app # per-pixel light, console verified
build/sysprobe/sysprobe-audio.app
build/doom_x7/doom.app
build/omega_x7/omega-race.app
```

ELF structure and disassembly reports are generated beside the binaries.
Builds check the 243 SYSCFG syscall stubs; the app additionally checks its
process/thread and application lifecycle stubs against calculat.app, its ELF
layout, and collisions with supplied firmware ELF load ranges.

## Build a single package

```sh
cmake -S libg1_probe -B build-libg1
cmake --build build-libg1 --parallel

cmake -S sysprobe -B build-sysprobe
cmake --build build-sysprobe --parallel
```

These place binaries directly in the respective build directories. Both
packages also retain their `build.sh` script, which builds in the package's
source directory using the same ABI and validation checks.

`-DPROBE_YIELD=OFF` disables the libg1 checkpoint yields (default ON).
`-DAPP_BASE=0x69800000` sets the app's fixed base (shown default); supplied ELF
overlaps are rejected. For another compiler program prefix, configure a fresh
build with `-DX7_TOOLCHAIN_PREFIX=...`. CMake's cache settings are independent
of the shell scripts' `PROBE_YIELD`, `APP_BASE`, and `CC` environment variables.

See [`syscalls.md`](syscalls.md) for the recovered ABI and
[`sysprobe-results.md`](logs/sysprobe-results.md) for console filesystem results.

## Source checkout and local inputs

The external `DOOM/` clone, WAD/ROM set, ROM archive, returned diagnostic logs
and game-save sample remain local inputs and are ignored for check-in. Keep them
to build/test the games; clone the original engine under `DOOM/` in a new checkout.
Firmware references in `binaries/` and `SYSCFG.SYS` are also required by validation
and retain their existing ignore rules. Original dumper binaries/reference inputs
are preserved; generated backlight patch outputs can be recreated with its script.
No build, installation, resource migration or launcher change happens at check-in.
Build/install instructions are in each package README.
