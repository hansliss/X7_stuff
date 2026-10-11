# X7 input investigation

Current checkout: source-only; rebuildable binaries are not bundled. The games
now have independent launcher entries and console-confirmed resource loading
from `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/`; Calculator is restored.
Earlier binary/path/test directions below are historical. Current instructions
are in the game READMEs and root SESSION.md.

The recovered SDK names do not establish the console's physical button layout.
The current console log contains the previously ignored bit `0x00200000`.
Its first 32 events are insufficient to reconstruct every physical control.

## Confirmed software alias

`binaries/msg_serv.app` has a default enabled mapping table at `0x6940a024`
(after the two repeat-timing words in `key_para` at `0x6940a01c`). Two
20-byte records contain:

| Enabled | Input mask | Input type | Output mask | Output type |
| --- | --- | --- | --- | --- |
| 1 | 0x40 | 0x3f000000 | 0x100 | 0x3f000000 |
| 1 | 0x80 | 0x3f000000 | 0x200 | 0x3f000000 |

`key_mapping` at `0x69405ca4` applies this to controller/source 0. A type
of `0x3f000000` selects mask-only replacement, preserving the event type.
Thus matrix left/right and ADC inputs genuinely become indistinguishable
in the GUI messages delivered to DOOM. This establishes a software alias;
it does not establish that all physical controls are wired together.

`key_mapping_set_status` at `0x69405c6c` can enable/disable individual input
mask entries. The system-message handler dispatches message 4018 to that
function, reading a 20-byte payload whose first words are status and input
mask. Message 4043 copies a four-byte payload into the global mapping-enable
flag at `0x6940ac68`. The preferred fix is to use the service protocol and
restore defaults on exit, after confirming the sending API and reply behavior.
These controls affect the shared service and are not yet used by DOOM.

## Driver paths

`KEY.KO` matrix scan at `0xd4000734` reads active-low low-eight bits from
`0xb01a0004`. `_cal_keyval` at `0xd40005e0` converts them:

| Raw matrix bit | Logical mask |
| --- | --- |
| 0x01 | 0x400 |
| 0x02 | 0x800 |
| 0x04 | 0x10 |
| 0x08 | 0x40 |
| 0x10 | 0x2000 |
| 0x20 | 0x1000 |
| 0x40 | 0x20 |
| 0x80 | 0x80 |

ADC scan at `0xd4000240` takes six bits from `0xb0000020`, bits 18–23.
Its table at `0xd4001510` maps inclusive ranges:

| ADC value | Logical mask |
| --- | --- |
| 0–2 | 0x200 |
| 3–9 | 0x8 |
| 10–15 | 0x4 |
| 16–21 | 0x2 |
| 22–28 | 0x1 |
| 29–35 | 0x100 |
| 36–43 | 0x200000 |
| 44–50, 51–55 | 0 |

Values 61–63 are idle. These tables do not identify which physical button
is connected to each path. The driver combines scan results before the
message service applies aliases. Directly draining `/dev/key` or switching
its global read mode would interfere with the normal consumer.

## Playable v3 diagnostic

Gameplay mappings are unchanged from v2. The first 256 native events are
buffered and emitted with sequence numbers; per-bit press/repeat and release
counts cover the entire session, including unknown bits. Output is batched
on exit, with no filesystem writes during play.

For an ordered test, briefly press and fully release each direction in this
order: left D-pad up/down/left/right, left stick up/down/left/right, right
D-pad up/down/left/right, right stick up/down/left/right; then each shoulder
button. Skip any nonexistent control and report that omission. Wait roughly
one second between actions. Exit with HOME and copy `doom-x7-play.txt` back.
Do this before normal gameplay so the trace has an unambiguous sequence.

The returned save is 21078 bytes, named `X7 SLOT 1`, with `version 110`,
medium skill, episode 1/map 1, and the expected final `0x1d` marker. This is
header/marker inspection, not a console load test.

## Ordered console test: confirmed physical mapping

The returned v3 trace matches all 18 control presses in the reported order,
followed by Select, Start, menu navigation and Quit. No events were omitted.
All test controls produced distinct DOWN/SHORT-UP pairs.

| Physical control | Delivered mask |
| --- | --- |
| Left D-pad / left stick up | 0x10 |
| Left D-pad / left stick down | 0x20 |
| Left D-pad / left stick left / left shoulder | 0x100 |
| Left D-pad / left stick right / right shoulder | 0x200 |
| X / right stick up | 0x1000 |
| B / right stick down | 0x800 |
| Y / right stick left | 0x2000 |
| A / right stick right | 0x400 |
| Unlabeled bottom-left button (Home) | 0x200000 |
| Select | 0x4 |
| Start | 0x8 |

The app's assumed HOME bit 0x4000 was never observed, explaining why the
unlabeled button did not exit. The right stick duplicates action buttons,
not the left directional controls. The normal GUI stream cannot distinguish
left-stick movement from its D-pad equivalents or right-stick movement from
the matching face buttons. The known message-service horizontal alias can
also explain shoulders matching left/right; disabling it is still needed to
measure whether those pairs are independent before that layer. The ordered
test alone does not prove a hardware alias between shoulders and directions.

A practical fallback is holding the physical Home button as DOOM's strafe
modifier, retaining Start/menu/Quit for exit. This needs no shared-service
changes and permits forward movement plus strafing if simultaneous inputs
are delivered. Simultaneous input requires a separate hardware check; the
message service has filtering that can suppress newly introduced combined
presses. The longer-term approach remains recovering the service command API
and temporarily disabling the horizontal aliases.

## V4 implementation

The named vendor API is `sys_forbid_key_mapping(int)`, group 18 trap index
0xb9, stub at `emulator.app` address `0x6880d2f0`. Emulator entry calls pass 1
(e.g. `0x68802ce8`, argument in the delay slot); cleanup calls pass 0
(e.g. `0x6880791c`, before applib quit). V4 follows this existing lifecycle,
logging results and restoring on initialization failures as well as exit.
This supersedes the earlier statement that DOOM does not change service
settings. The expanded v3 trace remains enabled.

## V4 console verification

The repeated ordered test delivered left D-pad/left-stick horizontal masks
0x40 and 0x80, and independent left/right shoulders 0x100 and 0x200.
Both `sys_forbid_key_mapping` calls returned 1. The user confirms shoulders
strafe. This resolves the horizontal alias question without a firmware patch.

Right-stick directions continue to duplicate the four face buttons, and
left-stick directions continue to duplicate the left D-pad. Home remains
0x200000. No trace records were omitted.

Events 42–50 cover a left-shoulder hold (DOWN, LONG, six HOLDs, HOLD-UP),
all carrying only 0x100. No forward event or combined mask appears in that
interval. The only third forward press is event 53 after Start, consistent
with menu navigation. Thus the requested forward-plus-shoulder test did not
establish simultaneous delivery. If the user pressed forward while holding
the shoulder, suppression is upstream of DOOM, consistent with the previously
identified `process_key_event` filtering. No input implementation change or
new binary was made on the strength of this result.

## Simultaneous input filter and V5 bypass

User confirmation establishes that forward added while holding a shoulder
is suppressed. `process_key_event` at 0x69405ac4 checks saved_key_val_app:
when nonzero, `0x69405b28` is reached with a0 ANDed with the saved mask
(delay slot at 0x69405af0). When no key is saved, it counts bits and rejects
samples with two or more bits at 0x69405b18–0x69405b24. This is independent
of the mapping flag.

KEY.KO mode 1 reads full 12-byte queue records, including complete held masks.
At 0xd4000e54–0xd4000e64, the number of records is floor(requested bytes/12).
The service's key_get_drv_keyval always requests four bytes (0x694005f0),
so mode 1 does not dequeue records for that consumer. DOOM requests exactly
120 bytes, the full ten-record capacity, and synthesizes state transitions.
The driver queues held samples and a zero-mask record on final release.

The stock emulator selects modes through `/dev/key` ioctl 0x4003 using scalar
arguments (operate_key_drv at 0x68808a80), and restores mode 0 on exit.
V5 adopts that lifecycle, restores on errors, ignores nonzero controller
sources, and keeps the raw turn/strafe mappings verified in V4.
Normal GUI key events cannot reset service idle timers while mode 1 is active;
this is an additional behavior to verify on the console with its existing
backlight configuration. No automatic-timeout syscall is added in this build.

At the start of this investigation the file present in the workspace was
still the 56-event V4 ordered trace; no separate additional-test log was found.

## Additional V4 log

The subsequently supplied `doom-x7-play-2.txt` is a V4 run with 37 trace
records and no omitted events. Events 3–25 show a prolonged left-shoulder
hold, delivering only 0x100 through DOWN/LONG/HOLD/HOLD-UP. There is no
forward or fire event during that hold and no combined mask anywhere in the
trace. The zero-mask/type event at index 2 is a no-op for current translation.
This corroborates upstream suppression, without identifying the exact times
or order of the additional physical presses. V5's raw-driver test remains the
appropriate next check.

## V5 console result: simultaneous controls work

The returned V5 log contains 287 raw records, including 143 combined states,
zero ignored controller records, and no omitted transition events. Forward
plus right shoulder works in both press orders (events 0–7), as does forward
plus fire (8–15) and forward plus run (16–22). Forward/right-shoulder/fire
are simultaneously held at events 25, 31, 37 and 43, reconstructed as mask
0x610. Recorded three-button press orders include forward/fire/shoulder,
fire/forward/shoulder and shoulder/forward/fire; exhaustive permutations are
not claimed. Releases preserve the other held controls until their release.
The user confirms the combinations behaved correctly on screen.

Raw-mode selection/restoration both returned 0; mapping disable/restoration
both returned 1. The app exited through the menu with reason 2 and DONE.
The final A press has no release because confirming Quit ends the app; this
is expected. No new binary is needed on the basis of this successful test.

### v12 toggle debounce

Start (`0x8`), Select (`0x4`) and the shoulders share the ADC ladder.
Only Start/Select presses require 50 ms of continuous reported state; release is
immediate. Raw snapshots are processed in full before pending presses mature,
so queued historical pulses cannot become toggles. Held-state checks also run
when the queue is empty. Other controls retain immediate snapshot translation.
The exit log records accepted toggle presses and rejected short pulses.
