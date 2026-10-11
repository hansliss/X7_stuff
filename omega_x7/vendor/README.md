# Source provenance

The old cycle-stepped CPU and unlinked reference headers have been removed
from the source package. V1/v2 used Andre Weissflog's zlib-licensed core at
https://github.com/floooh/chips/blob/master/chips/z80.h; v3 and later use the
instruction-based MIT core below.

`machine.c` adapts tone/noise/envelope timing from Andre Weissflog's
https://github.com/floooh/chips/blob/master/chips/ay38910.h (downloaded 2026-10-11)
to a local integer implementation. `ay_shapes.h` contains its envelope shapes.
The original copyright and zlib notice are retained in `CHIPS-LICENSE`.
The original amplitude table credits true-grue/ayumi; our 12-bit table rounds
those documented normalized levels. The unlinked full reference header is
no longer shipped.

Board memory/I/O maps, clocks, spinner encoding and ROM CRCs were checked
against Bernd Wiebelt's MAME Omega Race driver (BSD-3-Clause):
https://github.com/mamedev/mame/blob/master/src/mame/bally/omegrace.cpp.
Vector instructions and rate-multiplier rounding were checked against
Mathis Rosenhauer's BSD-3-Clause DVG implementation:
https://github.com/mamedev/mame/blob/master/src/devices/video/avgdvg.cpp.
The application implements its own instruction-level DVG decoder; it does
not compile or depend on MAME. The CPU and adapted AY portions retain their
upstream license terms.

z80.h SHA-256: `6ca70ffd91b1bdbaf00f7a41e7092b326145fd1c95dcdd7bb01183370de734f3`.

ay38910-reference.h SHA-256: `17d88a078d197f8fcfa9b3db5881aba01a6e861f0ac1c477045eb3eca4a4afb8`.

## v3 CPU core

`fastz80/z80.c` and `fastz80/z80.h` come from Nicolas Allemand's
https://github.com/superzazu/z80, downloaded 2026-10-11. The MIT notice is in
`fastz80/LICENSE`. Local adaptations remove the stdio/debug-output dependency,
route unsupported-op diagnostics to a board counter, and correct IM0 interrupt
acknowledge overhead to two T-states in addition to the supplied opcode's
normal timing. The isolated IM0 timing is tested.

The two CPUs now execute instructions atomically. The board scheduler retains
remaining instruction clocks across calls and interleaves CPU, AY, interrupt
and audio-sample events at their original nominal rates. Sub-instruction bus
edges are no longer modeled. A full-ROM regression compares monolithic runs
against runs divided into 723-clock chunks, including RAM, vector lists and
PCM contents. The older cycle-stepped core is no longer shipped.

Adapted fastz80/z80.c SHA-256: `bd93894cc5e380d0315a82a9d482442a0b12d43325ff42c3301ea1444376e98a`.

Adapted fastz80/z80.h SHA-256: `e44a45a0fc9cc10a8776329f278c0059c01e38fce8e24803cd2ad254079e7247`.

Adapted fastz80/LICENSE SHA-256: `e1fe7a1f2c90220ad9eff4bdcd3b4059f3897101bcd924c208bfcbe8c38ca6d5`.
