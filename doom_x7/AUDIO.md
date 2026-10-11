# Historical DOOM audio experiments

Current checkout: source-only; rebuildable binaries are not bundled. The games
now have independent launcher entries and console-confirmed resource loading
from `/mnt/diska/res/doom/` and `/mnt/diska/res/omega/`; Calculator is restored.
Earlier binary/path/test directions below are historical. Current instructions
are in the game READMEs and root SESSION.md.

The demo target/binary and recording are no longer shipped here. The recording
is retained at `../logs/doom/doom_audio.wav`. Commands, gain comparisons and
version-specific instructions below describe earlier experiments; current
usage/build instructions are in [README.md](README.md) and [PLAY.md](PLAY.md).
The final sample-scale correction and queue findings remain relevant.

# Normal-speed DOOM demo with sound effects

Ready binary: `doom-x7-audio.app`, **v5 deeper audio queue, with gain comparison**.

This build cycles through lower gains, with **1/2**, **1/3** or **1/4** displayed
in the black margin above the game. The gain applies to the correctly scaled 16-bit mix; system speaker
volume is left alone. Keep that system volume constant during the comparison.

| Time from gameplay start | Gain |
| --- | --- |
| 0–8 seconds | 1/2 |
| 8–16 seconds | 1/3 |
| 16–24 seconds | 1/4 |
| 24–32 seconds | 1/2 |
| 32–40 seconds | 1/3 |
| 40–48 seconds | 1/4 |

The 24-second cycle repeats for longer demos. Report which on-screen setting
sounds best, and whether choppiness persists even at 1/4. Different phases
contain different game sounds, so this is a quick comparison, not a controlled
replay of the same effect at every gain. START still exits early.

Gain changes ramp over one audio block (about 23 ms). Attenuation happens before
clipping, so lowering gain can prevent digital clipping rather than simply
reduce the volume of an already-clipped waveform. Per-gain log entries include
block counts, peaks before/after attenuation, raw overrange samples and clipped
samples. The log also records maximum time between pump calls and zero-queue
observations. These counters help diagnose gaps but do not prove driver underruns.
The on-screen gain describes newly mixed audio; buffered output can lag slightly.


Install it as `calculat.app` using the same replacement/launch procedure as the
silent prototype. Put the WAD at `/mnt/diska/apps/calculat/doom.wad`. Set `doom-x7-mode.txt` to
`11` (or remove that optional selector) for full-output DEMO1. The image remains
unscaled 320×200, centered on the panel. START cancels playback.

After loading and the five-second clock calibration, DEMO1 should take about
48.9 seconds, with gunshots, doors, monster sounds and other effects. DEMO2 and
DEMO3 are selectable using `12` and `13`, and run about 67.1 and 110.4 seconds.
The successful backlight-duration patch should allow these longer runs without
periodic stick movement. This app does not itself change global power settings.

Return `/mnt/card/doom-x7-audio.txt` and report whether the demo speed looks
normal, sound stays synchronized and free of gaps/crackling, and launcher return
works. This log is separate from `doom-x7-benchmark.txt`; the silent benchmark
binary remains available. The CRT failure log is still `doom-x7-start-error.txt`.

## What changed

The engine runs its normal 35 simulation tics per second, scheduled against the
calibrated OS clock instead of advancing as quickly as possible. Frames are
rendered once per simulated tic. Delayed callbacks catch up in bounded batches,
servicing GUI messages between callbacks. Timing excludes initial map loading.
The existing timedemo completion hook is retained to end the recorded demo,
but this target explicitly paces it; it is not an unpaced throughput benchmark.

The original engine `s_sound.c` now selects and updates effects, including
sound priorities and distance attenuation. The new backend reads and validates
DMX unsigned 8-bit sound lumps, caches independent samples outside the DOOM
zone, shares alias data, resamples with fixed-point phases and approximate
pitch ratios, and mixes up to eight voices into signed 16-bit values stored in
32-bit words for native precision-0 input.
The mono mix is duplicated into the two native API channels, matching the
single-speaker console and verified audio probe. Music is silent: no MUS/MIDI
synthesizer has been added. There is no stereo panning.

PCM uses the verified `libsound.so` format and buffer setup: two output channels,
44100 Hz, precision 0, sectors 2048, count 8. The shared frame callback services
audio even when no simulation tic is due. It targets 8192 queued frames (185.8 ms)
before adding another block and generates at most ten 1024-frame blocks per callback. Samples are
regenerated before submission because the native library can modify them.
Sound closes before library unload and runtime allocation cleanup, including
error paths. Existing system volume is retained.

The log adds sound starts, submitted audio blocks, queue polls, maximum queued
frames and audio pump ticks, plus the target simulation rate. A sound-start
count is not proof of audibility, and queue counters do not measure hardware
underruns. Coarse tick counters can undercount short mixer work. This version
has no gameplay controls beyond START; it remains a recorded-demo test.

## Build and validation

```
cmake -S doom_x7 -B /tmp/x7-doom-audio-build
cmake --build /tmp/x7-doom-audio-build --target doom-x7-audio -j4
python3 doom_x7/tests/run_host.py --audio
```

The CMake build checks the ELF load layout, entry point, undefined symbols and
vendor lifecycle/GUI syscall stubs. Host tests run the actual engine through
DEMO1–3, verify tic counts and 35-tic pacing, require nonzero in-range samples at the native 24-bit scale, model queue consumption and native input mutation, and check cleanup
including injected open/play/query/submit failures and early START exit. They
also exercise display comparison modes. The original silent integration suite
checks that the benchmark target still works. Host results establish logic and
cleanup; console timing, sound quality and combined performance remain to be
measured.

## v2 correction after the first console run

The first run had acceptable frame pacing, but the user reported almost no
sound until late in DEMO1, then generic noise, and red objects appearing blue.
The v2 build corrects two port errors:

- DOOM framebuffer pixels now put red in bits 11–15, green in 5–10 and blue
  in 0–4 (RGB565). The original packing swapped red and blue. The host image
  decoder is corrected too; its original matching swap hid the problem.
- The mono mixer is still calculated and clipped at signed 16-bit scale, but
  values are multiplied by 256 before native submission. Precision 0 is a
  24-bit-scale input in 32-bit words, not a 16-bit-scale input. The native
  conversion shifts words left eight bits (`LIBSOUND.SO` at `0x4e406e40`);
  its 16-bit output conversion reads the high halfword (`0x4e402728`). The old
  samples were 256 times too small. The host PCM fixture now verifies the
  required scale as well as bounds and models the native conversion.

The audible quality and color correction still need a second console run.

The first console log subsequently arrived: DEMO1 took 4954 OS ticks (49.54 s)
with all 1710 frames submitted and clean shutdown, but only **three audio
blocks** were submitted across 276 sound starts. Its maximum queue was 2232
frames. The original 1024-frame cutoff was too low for the native sector/queue
behavior and stopped feeding output. v2 restores the **2048-frame cutoff** used
by the successful probe and emulator. The host queue model now includes a
retained partial sector above 1024 frames, and full-run tests require sustained
submission, not merely one nonzero block. The precise native queue semantics
remain incompletely recovered; this change follows the console-tested caller.

## v2 console result and v3 validation

The user confirmed correct colors and sound throughout v2, but described it as
very loud and choppy. Its supplied log recorded 1792 audio blocks, 306 sound
starts, all 1710 frames and clean completion in 4983 OS ticks (49.83 seconds).
The submitted PCM totals about 41.61 seconds at 44100 Hz; the difference from
gameplay duration warrants checking service gaps, although these counters alone
do not establish the native output timing or prove missing audible intervals.

v3 keeps the verified output format/queue threshold and simulation pacing,
while comparing the three lower gains and recording the additional diagnostics.
Host DEMO1–3 runs exercise all gains with sustained output and zero software
clipping; this cannot rule out speaker/driver distortion or console service gaps.

## v3 console result and v4 correction

The v3 console run completed with 1760 submitted blocks and 306 sound starts.
The peaks after attenuation were 7828, 5371 and 4208 at gains 1/2, 1/3 and 1/4;
software clipping was zero. The user heard no appreciable difference in loudness
or quality. These counters described the mix before its erroneous ×256 scaling.

Tracing the final converter corrects the previous 24-bit-scale interpretation.
`LIBSOUND.SO` at `0x4e409508` uses a saturating shift by `16 - precision`.
Its general converter at `0x4e409998` clamps to the signed range determined by
`15 + precision`, then shifts by `16 - precision`. Thus precision 0 expects
signed 16-bit-scale samples in 32-bit words. The optional intermediate shift
at `0x4e406e40` also changes local precision to 8 at `0x4e406e6c`; it does not
justify pre-scaling input by 256. The high-halfword debug dump observes the
already converted output, not the caller's sample representation.

V4 removes ×256, retains the three gain phases and queue setup, and updates the
host mock to model the final conversion. Earlier statements above about v2's
24-bit input are superseded by this correction. Hardware audibility and any
remaining choppiness still require a console run.

Validation: the MIPS build and ELF/syscall checks passed. Host DEMO1–3,
rendering modes, and all 12 failure/cleanup scenarios passed with the corrected
sample bounds. These host checks do not establish speaker quality.

## v4 console result and v5 queue experiment

V4 removed the previous distortion, but playback still sounded like short
fragments with gaps. Its log reports 1736 blocks (40.31 seconds of PCM) over
48.89 seconds of gameplay, with maximum pump gaps of 150 ms. This strongly
suggests insufficient feeding, although firmware queue counters alone cannot
prove hardware underruns. V5 raises the queue target from 2048 to 8192 frames
and the refill limit from two to ten blocks. Expected buffered latency is about
186–209 ms, including staged data; actual driver buffering needs console testing.
The existing eight sectors and 2048-frame sector size remain unchanged.
The recording is `doom_audio.wav` (mono, 44100 Hz, 11.35 seconds).

The WAD path is now `/mnt/diska/apps/calculat/doom.wad`, including for builds of
the silent target. Logs and the mode selector remain on `/mnt/card/`. Faster
loading from internal storage is an experiment, not an established measurement.

Recording inspection: 10 ms RMS windows show an irregular envelope; the strongest
modulation peaks in the 5–25 Hz band are around 16–18 Hz. Relative to a local
250 ms maximum, low-energy intervals include approximately 90 ms at 1.77 s,
95 ms at 3.62 s and 105 ms at 6.30 s. Game effects naturally have pauses, and
microphone processing/background noise prevent these intervals from proving
underruns. The recording and console submission deficit support investigating
buffer starvation; they do not establish an exact ten-fragments-per-second rate.

V5 validation: MIPS build and syscall/ELF checks passed; host DEMO1–3,
all rendering modes and 12 failure/cleanup cases passed. A further run injecting
repeated 150 ms callback delays completed DEMO1 without exhausting the modeled
queue. Normal runs now require submitted PCM duration to reach at least 90% of
gameplay duration. These checks do not execute the native sound driver.

V5 console result: the user reported no choppiness, a brief delay during busy
action, and no noticeable latency in the last half. The returned v5 log has
2185 blocks (50.73 seconds of PCM) over 50.54 seconds of gameplay, an 180 ms
maximum pump gap, and 9160 peak queued frames (207.7 ms). This is the preserved
working demo baseline. The separate [playable prototype](PLAY.md) uses the same
queue target with fixed 1/2 gain and adds queue-depth histogram telemetry.
