# DOOM for the X7

`doom.app` is the completed playable port (v13), with music, sound
effects, persistent settings, saves/loads, configurable controls and a full-height
4:3 display. Console tests confirmed smooth gameplay and 3.6-second startup.
See [PLAY.md](PLAY.md) for installation, controls, file paths and usage.
This is a source-only package. The user confirmed its own launcher entry and
resource loading from `res/doom`, with the original Calculator restored.

All game assets and persistent files use `/mnt/diska/res/doom/`, independent of
the launcher app being replaced. Change `X7_DOOM_RESOURCE_DIR` in
[include/x7_paths.h](include/x7_paths.h) and rebuild to move the WAD, config and
all save slots together. Create the directory and move existing files manually;
the app does not migrate files or depend on its current working directory.
`prepare.py` emits references to this header, so it needs no path edits.

## Build

Requires CMake 3.20+, Python 3 and the GNU `mips-linux-gnu` compiler/binutils.
Run from the workspace root:

```sh
cmake -S doom_x7 -B /tmp/x7-doom-release -DX7_DOOM_DEBUG_LOG=OFF
cmake --build /tmp/x7-doom-release --target doom -j4
```

Output: `/tmp/x7-doom-release/doom.app`. Add a launcher entry for the built app
using the established launcher configuration procedure. The original Calculator
can remain installed; replacing it was only the initial test route. The top-level CMake build also
provides target `doom` and writes `build/doom_x7/doom.app`.
`X7_DOOM_DEBUG_LOG=ON` enables diagnostics; routine logging is OFF by default.
The obsolete silent/audio-demo targets and `build.sh` wrapper are removed.
Build directories stay outside this source directory. Companion `.app.dis` and
`.app.elfstruct` reports are generated in the build directory rather than shipped.

This directory is a port within the shared X7 workspace. It requires the
unchanged cloned engine at `../DOOM/linuxdoom-1.10`, plus `../cmake/`, `../tools/`,
`../syscalls.c`, `../syscalls.h`, `../sysprobe/`, `../SYSCFG.SYS` and `../binaries/`.
The last two hold firmware references used by the ABI/stub checks. `prepare.py`
generates adapted engine sources in the build tree and leaves the clone untouched.
The app is freestanding little-endian MIPS O32; it uses vendor syscalls and the
verified CRT/lifecycle/framebuffer helpers. Default load address is `0x69800000`.
An app filename change does not relocate it.

## Tests and source layout

Host tests remain separate from the native CMake targets and require native
`cc`, ASan/UBSan and the original `../DOOM/DOOM.WAD`:

```sh
python3 doom_x7/tests/run_host.py
python3 doom_x7/tests/run_music.py
```

The integration runner now tests the playable app by default (`--play` remains
an accepted compatibility argument). It checks real-engine gameplay, input,
scaling, settings, quiet logging and native-resource cleanup through firmware
mocks. Music tests cover the WAD scores, looping, timing, volume and bounds.
Native builds check all shared firmware stubs and ELF entry/layout. Host checks
are not measurements of native throughput or physical sound/input behavior.

Production sources are at the top level, interfaces/mappings in `include/`,
active music implementation and license notices in `vendor/`, and host checks
in `tests/`. No compiled app is shipped; rebuildable binaries, disassemblies,
cache files, the recording and obsolete synthesizer sources were removed.
The recording is preserved outside the package at `../logs/doom/doom_audio.wav`.
No WAD data is embedded or modified. Source provenance/licenses are in
[vendor/README.md](vendor/README.md) and the original engine repository.

## Findings and history

- [INPUT.md](INPUT.md): raw held-state input, firmware alias/filter discoveries
  and ADC toggle debounce.
- [AUDIO.md](AUDIO.md): historical experiments, corrected PCM scale and queue
  findings; old demo instructions are explicitly historical.
- [docs/HISTORY.md](docs/HISTORY.md): playable-port development and console results.
- [docs/PROTOTYPES.md](docs/PROTOTYPES.md): original feasibility/timedemo evidence.
- [Session findings](../SESSION.md): shared hardware and port decisions.

Reference resource-path build SHA-256: `7cb8516ebdccb7060effac96a26f284465b89250b9476d592ce767aa56ad365d`.
