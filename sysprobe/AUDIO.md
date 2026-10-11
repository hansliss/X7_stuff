# Simple PCM audio probe

This package is source-only. All current probe targets rebuilt successfully
before cleanup; binaries and inspection reports are generated, not bundled.
From the workspace root:

```sh
cmake -S sysprobe -B /tmp/x7-sysprobe-build
cmake --build /tmp/x7-sysprobe-build --parallel
```

Console observations and earlier calculator-replacement instructions below
remain historical evidence. DOOM and Omega now use their own launcher entries
and `res/*` resource directories; that game result does not establish a new
launch route for every probe.

Build `sysprobe-audio.app` using the commands below, then copy it onto the SD card and launch it through the same
launcher entry used for the other probes. No WAD or additional data file is
needed. Use a comfortable system volume before launching.

Expected behavior: a dark screen and three rising triangle-wave tones, roughly
440, 660 and 880 Hz, separated by short silent gaps. The waveform amplitude is
about 1/16 of signed 16-bit full scale. The probe does not change the system
volume or output routing. It exits automatically after roughly 6.5 seconds
(nominal 100 Hz OS clock), or after ten seconds if submissions have not finished.
START exits early. The normal duration is below the 30-second inactivity timeout.

The log is `/mnt/card/appprobe-audio.txt`. Return that file and report whether
sound was audible, clean, and increasing in pitch. A full run submits 176 blocks
of 1024 frames: 180224 frames, approximately 4.09 seconds at 44100 Hz, including
silence. The log records setup results, first submit result, queue polls, maximum
queued frames, elapsed ticks, and cleanup. A startup failure writes
`appprobe-audio-start-error.txt`. As with the other native probes, an unexpected
block inside a firmware call can prevent its automatic exit; the phase markers
are flushed before open/play/stop/close calls to identify that point.

## Recovered interface

`LIBG1.SO` (the GBA core) at `0x5146dfa0` loads `libsound.so` with flags 1.
Its stubs at `0x514cd3e0` through `0x514cd404` use syscall group 21:

| Slot | Recovered operation |
| --- | --- |
| 4 | Open output, no arguments; returns an opaque pointer |
| 5 | Command `(handle, command, argument)` |
| 6 | Submit `(handle, PCM descriptor pointer)` |
| 7 | Close `(handle)`; caller does not inspect a return value |

The probe follows the core's sequence: command `0x4d414f01` with an array
`{2, 44100, 0}` (channels, sample rate, precision), `0x4d414f32` with scalar
2048, `0x4d414f33` with scalar 8, and `0x4d414f02` with zero to play.
`0x4d414f03` pauses/stops playback before close. It deliberately leaves the
emulator's volume command `0x4d414f34` unused.

The PCM descriptor contains six pointers, then channel count, frame count,
and precision, all 32-bit fields (36 bytes). The emulator passes the same
sample pointer in the first two planes, two channels, 1024 frames, precision 0.
This duplicates a mono plane into stereo output: it is not an interleaved array
of 16-bit samples. `LIBSOUND.SO` at `0x4e40726c` detects identical plane pointers;
its conversion path at `0x4e406e40` operates on 32-bit sample words and can modify
input in place. The probe therefore regenerates its word samples before each
submission and never treats the input as immutable.

Queue command `0x4d414f45` takes an output pointer. At `0x4e402450`, the library
converts driver bytes into frames and adds its partial-sector frame count.
The probe follows the core's threshold: submit only when queued frames are
at most 2048. The library processes/copies the input during submit; static input
storage also remains alive until after audio close. Its internal driver and
playback thread manage the hardware buffer; the probe needs no direct DMA or
cache operations. The library uses `/dev/sound` and `/dev/snd_cfg` internally.

This remains a hardware probe: build validation cannot establish audible output,
firmware timer pacing, or successful cleanup on the console. A successful result
would establish a practical PCM output path for a DOOM software sound mixer;
this probe does not implement DOOM effects or music.

## Build

```
cmake -S sysprobe -B /tmp/x7-audio-build
cmake --build /tmp/x7-audio-build --target sysprobe-audio -j4
```

The resulting binary is `/tmp/x7-audio-build/sysprobe-audio.app`.
`sysprobe/build.sh` also builds this target alongside the existing probes.
The checked ready-to-copy binary is provided in this directory.

## Console result

The supplied `../appprobe-audio.txt` records a successful complete run, and the
user confirms audible sound through the console's single speaker. The stereo
API configuration does not imply two physical speakers; this probe sends the
same waveform to both channels. Independent left/right routing is untested.

- All format, buffer setup, play, first submit, and stop results are zero.
- 176 blocks / 180224 frames were submitted; 199 queue polls observed a maximum
  of 2304 queued frames (about 52.2 ms at 44100 Hz).
- The message-loop interval was 652 OS ticks, about 6.52 seconds at the nominal
  100 Hz rate. Exit reason 2 confirms automatic timer completion.
- Audio close returned, library unload succeeded, application lifecycle cleanup
  completed, and the log ends with zero assertion failures and DONE.

This validates the exercised native PCM path and cleanup. The log does not
measure underruns, exact audible sample rate, signal quality, or CPU cost when
combined with DOOM. A mono DOOM mixer can initially duplicate its output into
the two API channels using this verified configuration.

### Sample scale, corrected by final-converter tracing

Precision 0 expects signed 16-bit-scale values in 32-bit words. The final
LIBSOUND converter at `0x4e409508` saturates a shift by `16 - precision`;
`0x4e409998` explicitly clamps before the same shift. An optional intermediate
left shift by eight also updates precision to 8. The earlier interpretation
that callers must multiply 16-bit samples by 256 was incorrect and caused
saturation in DOOM v2/v3. DOOM v4 removes that multiplication. The original
probe's approximately ±2048 words were correctly scaled, about 1/16 full scale.
The original probe binary is retained.
