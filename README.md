# X7 syscall probes

## Documentation map

* [Syscall ABI](syscalls.md): both SYSCFG tables, arguments, 64-bit offsets,
  filesystem structures and confidence levels.
* [GUI, fonts and buttons](sysprobe/GUI.md): current library/API reference,
  message layouts, pixel packing and console-verified font dependencies.
* [Framebuffer and video](sysprobe/FRAMEBUFFER.md): driver ioctl structures,
  initialization/shutdown, pool mapping and the working buffer-ownership guard.
* [Rendering demos and measurements](sysprobe/DEMOS.md): all GUI/video demo
  generations, optimizations, observed artifacts, raw timings and limitations.
* [GUI investigation history](sysprobe/UI-notes.md): reverse-engineering
  evidence and the sequence of console experiments.
* [Filesystem console results](logs/sysprobe-results.md) and
  [watchdog findings](libg1_probe/README.md#watchdog-findings).

## Probe packages

The two probe packages are:

* [`libg1_probe/`](libg1_probe/README.md): the console-verified `libg1.so`
  loader probe, including filesystem, metadata, directory, and watchdog variants.
* [`sysprobe/`](sysprobe/README.md): the console-verified standalone `.app`
  probe, launched by installing it as `calculat.app`, with application lifecycle
  registration and clean exit.

Ready-built binaries remain in each package. Console result files and their generation directories are archived under
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
build/sysprobe/sysprobe-dodeca-fast.app # latest verified enlarged video demo
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
