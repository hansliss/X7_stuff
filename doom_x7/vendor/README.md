# Music source provenance

The inactive v10 Nuked-OPL3-fast sources were removed during distribution
cleanup. Its historical source was https://github.com/tgies/Nuked-OPL3-fast,
commit `f44bacb1143cd78d36bacf69fbec8c60a125b3c9` (LGPL-2.1-or-later).
The current app uses only the RP2040-derived implementation below.

`../music_tables.h` contains frequency and volume tables from Chocolate Doom
`src/i_oplmusic.c`, commit `895f581c5d91497bdda0516612da803fe5843e28`:
https://github.com/chocolate-doom/chocolate-doom.
Copyright 1993–1996 id Software and 2005–2014 Simon Howard;
GPL-2.0-or-later, license in `CHOCOLATE-COPYING`.
The GENMIDI register/frequency mapping in `../music.c` also follows that file.
The bounded MUS sequencer and X7 integration are local additions.

The original DOOM engine in this workspace carries its own original source
license. These additional components retain their licenses; this prototype
is a local build, not a replacement license for that engine or the WAD.
No WAD instruments or scores are embedded in the app.

## V11 renderer

`rp2040-opl/{emu8950.c,emu8950.h,slot_render.h}` come from
https://github.com/kilograham/rp2040-doom, commit
`29a453c980918a03e40fc8b69b024e7a3bdb5dc2`.
Copyright 2001–2020 Mitsutaka Okazaki and 2021–2022 Graham Sanderson;
MIT, with the license text retained in `rp2040-opl/LICENSE` and copyrights
in the source headers. This derives from emu8950 1.1.0.

Local adaptations include the X7 configuration header, the conditional host
assertion include, declaring the existing mono linear-buffer function, removing
an unguarded debug breakpoint, and committing pending pitch/key-scale changes
before key-on. That last fix prevents some attack rates becoming silent when a
new note raises the effective attack rate to 15.

The current build uses the portable C linear renderer (no ARM assembly), nine
voices, original 49716 Hz OPL timing, integer math, silent-voice skipping, and
no hardware timers, ADPCM, or hardware rhythm mode. DOOM percussion uses
ordinary GENMIDI voices. Music alone is linearly resampled to the existing
44100 Hz effects/output mixer with a constant divisor. Render batches are at
most 128 native samples and never cross a MUS tick.
