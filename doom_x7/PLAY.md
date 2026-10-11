# DOOM for the X7

Build output: `doom.app`, v13 final build (configurable controls; routine logging disabled).
Build the app and add its own launcher entry; this route and the resource
directory are console-confirmed. The original Calculator is restored. The earlier
calculator replacement procedure remains a historical test route.
Put the registered WAD at `/mnt/diska/res/doom/doom.wad`.
The app shows the title screen after loading and one-second clock calibration.
Press any button to open the main menu on a plain black background. New Game selects episode and difficulty. It keeps 35
simulation tics/second, renders at 320×200, scales output to 363×272, and uses
v5's larger audio queue. Effects use fixed 1/2 gain; there is no comparison
label. Music now plays through a nine-voice OPL2 synthesizer using the WAD’s
original GENMIDI bank. The menu’s music-volume setting controls it independently
of sound effects and persists across restarts.

| Control | Gameplay | Menu |
| --- | --- | --- |
| Up / Down | Forward / backward | Navigate |
| Left / Right | Turn | Adjust |
| Left / right shoulder (`0x100` / `0x200`) | Strafe left / right | Navigate |
| A | Fire | Confirm; yes in prompts |
| B | Use doors/switches; respawn after death | Back; no in prompts |
| X | Hold to run | — |
| Y | Next owned weapon | — |
| SELECT | Automap | — |
| START | Open menu | Close/back |
| Unlabeled Home (`0x200000`) | Unassigned | Unassigned |

Use START → Quit Game → A to exit. Tap menu directions;
held/repeat events do not repeatedly activate menu choices. A held gameplay
action is released when entering the menu and must be released and pressed
again before it can confirm a choice. All three native release classes clear
held state, and combined button masks are supported.

Controls and simultaneous movement/fire/strafe have been verified on hardware.
Start/Select presses require 50 ms stability to reject ADC transients; this also
passed the console test. Edit [include/x7_controls.h](include/x7_controls.h)
to change gameplay or menu mappings, then rebuild. The current assignments
are unchanged. Stick/D-pad aliases are imposed by the firmware.

Save/load use `/mnt/diska/res/doom/doomsav0.dsg` through `doomsav5.dsg`.
An empty save slot receives a name such as `X7 SLOT 1`; press A again to commit
it, or B to cancel. Saving replaces the selected slot, as in the original game.
Menu settings are saved on clean exit to `/mnt/diska/res/doom/doom.cfg`.
Save/load, map transitions and respawning have been verified on the console.

Create `/mnt/diska/res/doom/` and move the WAD, existing save slots and `doom.cfg`
there before running this build. Filenames and formats are unchanged; files are
not automatically migrated. The app does not create the directory. The sole
base-path definition is `X7_DOOM_RESOURCE_DIR` in
[include/x7_paths.h](include/x7_paths.h); edit it and rebuild to use another
location. Paths are absolute because launcher working-directory behavior is
unspecified. Diagnostic log and firmware-library locations are separate.

Routine logging is disabled in the final build. Quit normally to persist settings.
Debug builds write `/mnt/card/doom-x7-play.txt` with input, frame and audio
statistics. `doom-x7-mode.txt` does not select a demo or rendering mode for the
playable app.

## Display

The engine renders at 320×200. The backend uses precomputed nearest-neighbor
indices to fill 363×272, preserving DOOM's intended 4:3 CRT proportions on the
480×272 panel. This scaling has been verified on the console.

## Build and checks

```
cmake -S doom_x7 -B /tmp/x7-doom-release
cmake --build /tmp/x7-doom-release --target doom -j4
python3 doom_x7/tests/run_host.py
```

The playable target disables the old `NetUpdate` command-generation path,
including calls inside rendering. The GUI loop builds exactly one command for
each game tic. The playable game is the only native build target; the old demo targets
and binaries have been removed.

Validation: the MIPS build, ELF layout and firmware syscall checks passed.
Host gameplay moved the player and consumed pistol ammunition, exercised menu
open/close and weapon cycling, and exited cleanly. All 12 failure/cleanup cases
passed. Separate input checks cover simultaneous buttons, repeat suppression,
release classes, context changes, confirmation, run/use and automap. These do
not establish physical key delivery or native sound-driver behavior.


No prebuilt app is shipped in the source package. See README.md to rebuild.

Development history: [docs/HISTORY.md](docs/HISTORY.md).
