# Omega Race for the X7

Build output: `omega-race.app`, **v5**, with four-level antialiasing,
persistent high scores and
routine logging disabled. It runs the original external arcade ROMs through a
small dual-Z80 board emulator with software vector drawing and mono AY sound.
V3 was console-verified at full speed with working controls and sound. V4 added
persistence and quiet logging; v5 smooths the software vector drawing. These
final changes passed host and MIPS checks. The user has confirmed its own launcher
entry and loading ROMs from `res/omega`; separate final high-score and antialias
quality/performance reports have not been recorded. This package is source-only.

## Install and play

Build `omega-race.app` as described in [BUILD.md](BUILD.md), then add its own
launcher entry using the established launcher configuration procedure. Both games
can coexist with the restored Calculator; its replacement was the initial test
route. Copy all **eight** original ROM files,
with their existing names, into:

```
/mnt/diska/res/omega/
```

Put the eight files directly in that directory, without another `omega_race`
subfolder. The folder is required. The [ROM manifest](rom-manifest.txt) lists exact sizes
and CRCs; every ROM is validated before graphics/audio initialization. ROMs
remain separate and unchanged. Launch the Omega entry, wait for the original
attract display, tap Select to insert a coin, and Start to begin one-player play.

| X7 control | Action |
| --- | --- |
| Select | Insert one coin |
| Start | One-player start, one credit |
| Left / Right | Turn |
| A | Fire |
| B or Up | Thrust |
| Hold unlabeled Home for ½ second | Save changed battery RAM and quit |

The left stick aliases the D-pad; the right stick aliases the face buttons.
Start/Select require a stable 50 ms press. Gameplay combinations remain
immediate. Edit [controls.h](controls.h) and rebuild to change assignments or
the spinner turn step. There is no in-game settings, pause or two-player UI.

The square playfield fills the panel height: 272×272 centered at x=104 on the
native 480×272 RGB565 screen. White intensity-valued lines reproduce the
monochrome vector display. Four nonzero edge-coverage levels (¼, ½, ¾, full)
smooth lines while preserving the ROM's brightness levels. Fractional pixel
coordinates retain details previously rounded away. Cabinet art and phosphor
glow are omitted.

## High scores and storage

The original ROM manages its high-score table in the board's 256-byte
battery-backed RAM. V4 restores that entire region before running either CPU.
Changed RAM is saved on a normal Home or system exit to:

```
/mnt/diska/res/omega/omega-race.nvram
```

**Quit normally before powering off to retain new scores.** No filesystem writes
occur during gameplay. Unchanged RAM is not rewritten. Missing saves are normal;
invalid/unreadable saves leave fresh zeroed battery RAM for the ROM to initialize.
A versioned header, exact size and CRC protect loading. Writes go to
`omega-race.nvram.tmp`, are synced and closed, then renamed over the final file.
Write/sync/close/rename failures preserve the previous final file. A leftover
`.tmp` file is ignored on startup and can be removed. Initialization/error exits
do not save. Failed saves allow exit but emit a diagnostic if the SD card is
available. Atomic replacement does not guarantee survival of arbitrary power
loss inside the firmware/filesystem; normal exit is the supported path.

Back up the final `.nvram` file to preserve scores. Remove it while the app is
closed to reset battery RAM. ROMs are never written.

Create `/mnt/diska/res/omega/` before launching and move an existing
`omega-race.nvram` there to retain scores. No automatic migration or directory
creation is performed. ROMs and battery RAM derive their absolute paths from
the single `OMEGA_RESOURCE_DIR` definition in [paths.h](paths.h). Edit it and
rebuild to change their location; diagnostic log paths remain separate.

## Logging and final console check

The default build creates no routine `omega-x7.txt` and leaves existing logs
untouched. Runtime/startup failures may write `/mnt/card/omega-x7-error.txt`;
early CRT failures may write `/mnt/card/omega-x7-start-error.txt`. Invalid NVRAM
or a failed save also produces an error log, without stopping gameplay. These
files can be stale from previous failures; they are not cleared by a good run.
Diagnostics are optional: the app can run without a writable SD card.

For the final test, play long enough to enter a high score, quit with Home,
relaunch, and confirm the score remains. Also check combined turn/thrust/fire,
continuous sound and a normal return to the launcher. A new routine log is not
expected. Check small circles, ship outlines and text for smoother edges without
sluggishness or audio gaps. If something fails, copy back any newly modified
error log. Earlier comparison binaries have been removed during source cleanup.

## Documentation

- [BUILD.md](BUILD.md): release/debug builds, dependencies, artifacts and checks.
- [TECHNICAL.md](TECHNICAL.md): board model, native interfaces, persistence format,
  timing, rendering, limitations and decisions.
- [HISTORY.md](HISTORY.md): startup failure, CPU optimization and console evidence.
- [vendor/README.md](vendor/README.md): upstream sources, adaptations and licenses.
- [Session findings](../SESSION.md): DOOM, audio/input discoveries and timeout patch.

Host-rendered gameplay preview: [preview.png](preview.png).
Before/after comparison: [antialias-comparison.png](antialias-comparison.png),
v4 on the left and v5 on the right; attract text above and gameplay below.

Reference v5 resource-path build SHA-256: `0c94d595b44654d8f81346400dda13251d2a9f882a96ec3c81b2da71ff60404b`.
No compiled app is shipped; rebuildable binaries and inspection reports
were removed. Version hashes and measurements remain in HISTORY.md.
