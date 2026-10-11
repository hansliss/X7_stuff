# DOOM development history

Current checkout: source-only; rebuildable binaries are not bundled. The games
now have independent launcher entries and console-confirmed resource loading
from `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/`; Calculator is restored.
Earlier binary/path/test directions below are historical. Current instructions
are in the game READMEs and root SESSION.md.

Current resource paths are defined in `../include/x7_paths.h`, defaulting to
`/mnt/diska/res/doom`. Historical calculator-directory paths below are no longer
the shipped defaults. WAD/config/save names and formats remain unchanged.

Versioned binaries and demo targets have been removed. Earlier test directions
and paths below are retained as evidence, not current usage instructions.
Use [../README.md](../README.md) for canonical builds and [../PLAY.md](../PLAY.md)
for installation and controls.

## Console mapping correction in v2

The first playable run confirmed the action buttons and no noticeable audio
latency, but all tested horizontal controls strafed. Its first 32 captured
native events contain `0x100`/`0x200`, not `0x40`/`0x80`. V2 assigns turning
to `0x100`/`0x200` and strafing to `0x40`/`0x80`; both pairs navigate menus.
The SDK labels L/R and Left/Right do not match the observed physical mapping.
Controls that emit identical native masks necessarily perform the same action;
which physical controls produce the remaining horizontal pair needs testing.

## Firmware alias found in v3 investigation

The message service defaults to remapping `0x40`/`0x80` into
`0x100`/`0x200`. This explains the missing strafing pair in v2. V3 keeps
those gameplay mappings and adds 256-event traces and full-session per-bit
counts. See [INPUT.md](../INPUT.md) for disassembly evidence, the service controls
and an ordered physical-button test. No shared service settings are changed
in this diagnostic build. The returned save header and end marker match the
1.10 format; loading still needs console verification.

## V4: raw directional test

V4 calls the same `sys_forbid_key_mapping(1)` API used by the stock emulator
(group 18, index 0xb9), after applib initialization. Every normal exit and
initialization-error cleanup after that call invokes `(0)` before applib quits.
The log records both return values. Force-reset behavior is unverified.
This follows the vendor default-restoration sequence rather than querying
prior state; there is no permanent firmware patch.

Raw `0x40`/`0x80` now turn, while `0x100`/`0x200` strafe. Both pairs navigate
menus. All action buttons retain their previous assignments. The unlabeled
Home remains unassigned so it cannot interrupt the ordered test.

Repeat the same button sequence as the previous test, then exit using
Start → Quit Game → A and return the log. Afterwards, try turning with the
left D-pad/stick and strafing with each shoulder. If those work separately,
try forward plus a shoulder to check simultaneous input delivery.

Build verification includes a byte-for-byte comparison of the new syscall
stub against `emulator.app`. Host tests check balanced disable/restore across
normal play and all existing initialization-failure cleanup scenarios, plus
raw turning/strafe translation. They do not establish actual hardware masks.

## V4 console result

The ordered console trace confirms independent left/right directional masks
`0x40`/`0x80` and shoulder masks `0x100`/`0x200`. The user confirmed that
shoulders now strafe. Left stick still duplicates the left D-pad; right stick
still duplicates X/B/Y/A. Mapping disable and restore both returned 1, and
all events fit in the trace.

Combined forward-plus-shoulder input is not confirmed. During the subsequent
left-shoulder hold, the trace contains only `0x100` down/long/hold/up events;
there is no additional forward press or combined mask. The extra forward
press occurs after Start and appears to be menu navigation. If forward was
pressed during the shoulder hold, the firmware suppressed it before delivery
to this app. App-level translation supports combined and overlapping events,
but it cannot reconstruct a press absent from the GUI stream.

## V5: simultaneous controls

The service's `process_key_event` at 0x69405ac4 locks onto its first accepted
button, ANDing subsequent state samples with that button and rejecting
initial multi-button samples. Disabling key mapping alone cannot remove this
filter. V5 selects KEY.KO mode 1 using ioctl 0x4003 and consumes full 12-byte
records from `/dev/key`. Records include source/controller, complete held mask,
and four additional bytes. Only built-in source 0 is translated.

The service requests four-byte reads; mode 1 requires a complete 12-byte
record, so those reads return zero without dequeuing a record. This prevents
competition with the service. V5 synthesizes presses and releases by comparing
successive masks, preserving held controls as others are pressed or released.
GUI key events are ignored while this raw path is active. Reads are bounded
to the driver's ten-record queue, with polling during calibration and gameplay.
Cleanup restores driver mode 0, closes the descriptor, and restores mapping.
No persistent firmware changes are made.

The log adds raw-record, combined-state, and ignored-controller counts,
and results for selecting/restoring driver mode. Existing input trace entries
now describe synthesized transitions rather than native GUI event classes.
Empty queues return -1 in this driver; V5 treats nonpositive reads as no data.
The GUI service no longer sees gameplay presses in raw mode, so its ordinary
key-driven idle-timer resets are bypassed too. Audio and rendering are unchanged.

Test each pair in both orders, holding both for about one second:
forward + left shoulder; forward + A; forward + X. Release one button and
confirm the other keeps working, then release the other. Try forward +
left shoulder + A together. Exit with Start → Quit Game → A and return the log.
Home remains unassigned. Console validation is still required.

V5 validation: MIPS build and syscall/ELF checks passed. All 12 existing
failure/cleanup cases and host gameplay passed with balanced mapping and raw
mode restoration. The gameplay fixture delivered overlapping forward/shoulder
states; standalone tests verified adding fire to that combination, releasing
one control while retaining the others, and final release of all controls.
V4 is preserved as `doom-x7-play-v4.app` for comparison.

V5 console verification: the user confirmed combined movement, strafing,
firing and running. The log contains 143 combined states among 287 raw
records, confirms both press orders for the tested two-button pairs, and
shows four forward/right-shoulder/fire combinations. Normal driver mode and
firmware mapping were restored successfully on menu exit. Input combinations
are now validated on the console; exhaustive three-button permutations were
not required or established.

## V6: startup menu and scaled display

V6 removes the forced `-skill 3 -warp 1 1` arguments. It initializes the stock
title page and opens the main menu; New Game, Load Game, Options and Quit are
available. The playable title page stays static when the menu is dismissed,
so no automatic attract demo starts. Start reopens the menu. Registered DOOM
has its normal three episodes and five skill choices.

The stock 320×200 framebuffer is converted directly into centered 363×272
RGB565 output using precomputed nearest-neighbor row/column indices. This
reproduces the intended 4:3 proportions to the nearest pixel. Side margins
are 58 pixels left and 59 right. No extra image allocation or per-frame division
is required. Demo builds retain their existing unscaled output.

The conversion writes 98736 pixels instead of 64000 (about 54% more).
Console timing still needs measurement; the log retains palette-conversion,
rendering and audio timing counters and now records output dimensions and
startup menu state. V5 is retained as `doom-x7-play-v5.app` for comparison.

Check the startup menu, choose New Game and a difficulty, then play normally.
Please also verify Load Game from the startup menu and return the log after
exit. Look for comfortable proportions, readable text, full-height status bar,
and whether the extra conversion work affects frame rate or sound.

Save loading, map transitions and respawning have already been confirmed on
the console by the user. Music and persistent menu settings remain future work.

V6 validation: MIPS build and syscall/ELF checks passed; all 12 existing
failure/cleanup scenarios passed. Host gameplay confirmed startup menu and
title state, selected New Game/episode/skill, moved and fired, and checked
simultaneous input and exit. Scaler tests verified every output pixel against
source coordinates, side-margin preservation and guards on both ends of the
framebuffer. Native scaling performance and startup Load Game remain console
checks.

## V7: menu readability and weapon cycling

The original engine draws menus over the title image or gameplay. V7 clears
the startup/title-menu background to black before drawing menu items and
prompts. Gameplay menus retain their existing backdrop; dismissing the startup
menu still reveals the title picture. Full-height scaling is unchanged.

Tap Y (or right-stick left, its equivalent) to cycle owned weapons. The prior
code posted numeric-key press and release together, so DOOM cleared the key
before G_BuildTiccmd could encode BT_CHANGE. V7 holds the synthesized key for
one game tic, then releases it. A held Y does not repeatedly cycle. The host
integration check verifies an actual pistol-to-fist switch, rather than just
the presence of key events.

The V6 console log shows zero mixer clipping/overrange samples. This does
not establish the cause of the reported occasional hiss; sound generation
is unchanged in V7. V6 is retained as `doom-x7-play-v6.app`.

V7 validation: MIPS build and syscall/ELF checks passed, along with all 12
failure/cleanup cases, startup-menu gameplay, actual weapon selection,
input translation and complete scaled-frame bounds checks.

## V8: title-screen startup and quit audio

Startup displays TITLEPIC and waits indefinitely for the first built-in button
press. That press opens the menu; it and its release are consumed so A cannot
both open the menu and select New Game. The menu retains V7's black background.
The startup log records `startup waits for key=1` and `startup menu active=0`.

Original M_QuitResponse plays one of eight randomly chosen sound effects,
then calls I_WaitVBL(105), a 1.5-second wait. V8 continues refilling audio once
per OS tick during that wait. Previously it blocked refills and could exhaust
the roughly 200 ms queue. This fixes a known starvation opportunity at final
Quit confirmation; it does not establish the cause of hiss during navigation.
The V7 console log has no clipping or zero-queue observations and a maximum
pump gap of 400 ms, without event timestamps that locate the reported hiss.

Host gameplay now wakes the title, selects New Game, cycles a weapon, and
quits through the actual menu/confirmation path instead of the legacy exit
mask, covering the quit-sound wait. V7 is preserved as `doom-x7-play-v7.app`.

V8 validation: MIPS build and syscall/ELF checks passed; all 12 existing
cleanup/failure cases passed. Host gameplay woke the title, selected New Game,
changed weapons, and completed menu Quit confirmation. Input and scaled-frame
checks passed. The host quit wait retained a maximum audio refill gap of
one modeled OS tick; native audio behavior still needs confirmation.

## V9: startup time and persistent settings

Calibration is shortened from five seconds to one second for the playable
build. Engine read checkpoints yield once every 32 reads or after 100 ms of
elapsed OS time, rather than unconditionally yielding after every read.
Startup output is buffered in 8 KiB chunks instead of syncing every log line.
The buffer is flushed when calibration finishes and on initialization errors.
Demo builds keep their original calibration and loading-yield behavior.

The cold-start host model has 1338 read checkpoints and 42 loading yields.
It reaches the end of calibration at 158 modeled OS ticks (1.58 seconds at
100 Hz), versus roughly 18.5 seconds of explicit delays before optimization.
This excludes real console storage/CPU costs and the launcher's loading of
the app. Console measurement is required. Logs now include startup elapsed
ticks, read/yield counts, and timestamps on BEFORE checkpoints.

Settings are stored at `/mnt/diska/apps/calculat/doom.cfg`, alongside the WAD
and saves. The bounded, versioned text format persists mouse sensitivity,
effects volume, music volume, message visibility, screen size, detail level,
and gamma. Music volume is retained for the upcoming music implementation.
Only these numeric options are accepted, with their engine-supported ranges;
unknown, malformed and out-of-range entries are ignored. Missing, unsupported
or oversized configuration files fall back to defaults.

Settings are written on normal exit, including menu Quit and system exits.
Initialization/error exits do not write settings. The write uses `doom.cfg.tmp`,
syncs and closes it, then renames it into place. Failed writes or replacements
keep the old configuration. The log reports loaded-entry count and save result.
There is no key rebinding UI or additional display-mode setting in this version.

Console test: measure launch-to-title time, change effects volume, Messages,
and Screen Size in Options, then Quit normally. Relaunch and confirm those
choices remain. Return `doom-x7-play.txt` after the second exit; the settings
file can also be copied back if useful. V8 is preserved as
`doom-x7-play-v8.app` for comparison.

Validation: MIPS build and firmware syscall/ELF checks passed; all 12 existing
failure/cleanup scenarios, title/menu gameplay, weapon switching, simultaneous
input and scaling checks passed. Settings tests checked fresh defaults,
nondefault restart round-trips, malformed/out-of-range entries, unsupported
versions, oversized input, and a failed rename preserving the old file.

## V10 music test

Install the new `doom-x7-play.app` as `calculat.app`. V9 is retained as
`doom-x7-play-v9.app`. Listen for title music, start E1M1 and check the track
changes. Move, fire and enter the menu while music plays. Change music volume,
including zero, and check that effects remain audible. Quit normally so settings
and diagnostic counters are written. Please retain `doom-x7-play.txt` and report
any crackling, pauses, or loss of game speed, especially during busy fights.

MUS timing runs at 140 ticks/sec from the shared 44100 Hz sample clock. Music
uses integer-only Nuked-OPL3-fast in OPL2 mode, nine melodic/percussion voices,
mono output and conservative gain before the existing master attenuation. The
detuned second GENMIDI voice is added for double-voice instruments when a
voice is available; allocation steals the oldest active voice when full. This is AdLib-style music,
not exact DMX voice-priority emulation or a General MIDI soundfont.

Host checks render every MUS lump and loop every distinct score, validate pause/resume and
volume, exercise malformed input under ASan/UBSan (leak scanning is disabled
under the command runner’s ptrace), and run the real-engine
movement, menu, weapon, configuration and cleanup fixtures with music enabled.
These checks do not establish X7 synthesis performance; the console log retains
queue, audio-pump timing, clipping, music event and error counters for that test.

Music-only checks: `python3 doom_x7/tests/run_music.py`.
Full integration: `python3 doom_x7/tests/run_host.py --play`.
Source and license provenance: [vendor/README.md](../vendor/README.md).

## V11 music performance and volume

The v10 console log confirms the slowdown: 14,447 simulation ticks over about
969 seconds, or 14.9 ticks/sec against the 35 target. Audio pumping consumed
68.3% of that interval; display work consumed 16.1%. There were no recorded
empty audio queues or clipped samples. Music peak at the previous maximum
setting was only 2,641 before the mixer’s 1/2 master gain.

V11 switches from Nuked’s sample-by-sample chip renderer to RP2040 DOOM’s
portable C implementation of emu8950, rendering each voice in short batches.
Native chip rate and MUS timing are retained; effects still use the same
44100 Hz mixer and gain. Mono conversion uses one constant-divisor interpolation
instead of Nuked’s four-output resampler. Music gain is raised fourfold.

A host `-O2` comparison rendered the same 60 seconds of E1M1 music in 0.800 CPU
seconds with v10 and 0.184 seconds with v11, roughly 4.3× faster. This is a
relative host measurement, not a prediction of X7 frame rate. Audio character
may differ slightly between the two OPL implementations.

Install the current app as usual and try a busy fight. Start with music volume
around the middle of the slider and increase it as desired. Check responsiveness,
music/effect balance, and distortion at the highest volume. Quit normally and
retain `doom-x7-play.txt` for comparison. Both `doom-x7-play-v9.app` (effects only)
and `doom-x7-play-v10.app` (first music build) are preserved.

Current music CPU benchmark: `python3 doom_x7/tests/run_music.py --benchmark`.

V11 validation passed: all 45 MUS entries rendered, every distinct score looped,
pause/resume preserved exact sample progression, and volume/controller/bounds
checks passed under ASan/UBSan. Maximum music peak over those scores at full
volume was 14,986 before the master gain. The real-engine gameplay/configuration
fixture produced no clipped samples. X7 entry, syscall stubs, load segments and
address-range checks also passed. The final binary SHA-256 is
`672ae0cb6af7d777a4a69df9bf0a3f7b3258ce795eb511f4d43c361392e75ab2`.

## v12: Start/Select stability check

The v11 hardware session confirmed good music volume and performance. Its input
trace also reported unexpected Start and Select presses following right-shoulder
releases. These buttons share an ADC ladder in KEY.KO, so transient readings are
a plausible cause, though the previous trace has no pulse timestamps.

Start and Select now require a stable 50 ms press. Releases and gameplay buttons
remain immediate, including simultaneous movement, fire and shoulder strafing.
The filter checks held state even when no new driver records arrive and cancels
short pulses within a queue batch. Exit logs count rejected pulses and accepted
UI presses. Test normal Start/Select taps and repeated right-shoulder releases
during play, then exit normally and copy the log back.

v12 app SHA-256: `cd57d5aba521c7b2e67532e5fdb16201828b61f8666bed7acab6d3bfeb75a8bc`. Host input, gameplay, display, settings and cleanup
checks passed; the MIPS app layout and firmware syscall checks passed.

## Final build (v13)

The console tests confirmed WAD loading, title/menu flow, full-height scaling,
simultaneous controls, saves/loads, level changes, respawn, independent music/SFX
volume and persisted settings. Startup was measured at 3.6 seconds. The optimized
music played smoothly at an appropriate volume. Start/Select filtering prevented
unexpected menu/map openings while normal Start presses still worked.

Gameplay and menu button assignments now live in `include/x7_controls.h`.
Change the action macros there (OR masks to bind several buttons; 0 disables an
action), then rebuild. Next-weapon remains an edge-triggered action. Start/Select
physical buttons retain their ADC stability check regardless of assignments.

Build from the workspace root:

```sh
cmake -S doom_x7 -B /tmp/x7-doom-release -DX7_DOOM_DEBUG_LOG=OFF
cmake --build /tmp/x7-doom-release --target doom-x7-play -j4
```

The resulting `/tmp/x7-doom-release/doom-x7-play.app` can be installed as
`calculat.app`. To restore prototype diagnostics, configure with
`-DX7_DOOM_DEBUG_LOG=ON`. The final build creates no routine SD-card log; early
CRT failures may still create `doom-x7-start-error.txt`. Existing old logs are
left in place. The previous console-tested app is retained as
`doom-x7-play-v12.app`.

v13 app SHA-256: `a9e865477e4927fbc18dbe5e835ce67866d07b6320c769dff89745c6d2f0706e`. Input, playable engine, scaled display, settings,
cleanup and logging-disabled host checks passed. MIPS app layout and firmware
stub checks passed. The final refactor awaits installation on hardware; its
control assignments and debounce behavior match the tested v12 build.
