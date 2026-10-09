# NAND dump library with scheduler yields

`libg1.c` is the replacement LIBG1 loader hook for the original NAND dumper.
It reads from LBA `0x22000` for `0x200000` sectors (1 GiB), producing 64 files
`/mnt/card/dump000.bin` through `dump063.bin`, each **16 MiB**. The former
4 MiB comment was incorrect: `FILE_SECTORS=0x8000`, with 512-byte sectors.
NAND reads and SD writes are 32 KiB each. Completion markers are named
`dumpNNN.ok` and contain `OK\n`.

## Yield experiment

The hook now calls the verified vendor `OSTimeDly(1)` trap (group 1, slot 16):

- After every 16 successful chunks, i.e. every 512 KiB within a file.
- After closing each dump file and after closing its completion marker.
- When skipping an already completed file.

This releases the CPU briefly so other tasks can run. It does **not** directly
feed or disable the watchdog. Per-file delays alone would leave an entire
16 MiB write between scheduling opportunities, so intermediate yields are
also included. `YIELD_CHUNKS` controls that frequency; its current value is 16.
The wall-clock duration of one OS tick is not established.

The [watchdog investigation](../../libg1_probe/README.md#watchdog-findings)
found separate device registrations and application software watchdogs.
Opening and feeding a new `/dev/wd` descriptor would not refresh the manager's
existing registration. The loader hook still performs all work synchronously
inside `_init`; yields cannot make a blocked callback in that same thread run.
A long NAND/SD syscall can also prevent the next yield. Consequently this
change is a hardware experiment, not a guarantee that the entire dump survives.
If it still resets, the next investigation is worker-thread/lifecycle behavior
or the loading application's watchdog, rather than feeding a new descriptor.

## Completion and failure handling

The hook checks for a missing `nand_adfu_read` symbol and failed allocation
before starting. Filenames now use writable arrays rather than modifying
string literals. A short SD write stops the dump and does not create an `.ok`
marker. Marker checks require its first three bytes to be `OK\n`, so an empty
or truncated marker does not cause a skip. A short marker write also stops.
Buffers are released on normal loop exit and write/open failures.

The NAND reader's return semantics remain unverified; this change preserves
the original read usage. Markers do not verify NAND correctness, file length,
SD persistence or content checksums. Valid markers created by the old code may
still belong to incomplete files, because that version could write `OK\n`
after a short data write. Verify those old files are 16 MiB and remove the
corresponding markers for any incomplete files before relying on resume.
Files without an accepted marker are truncated and dumped again.

## Build and validation

The existing build script requires its own directory as the working directory:

```sh
cd dumper/libg1_dump
bash build.sh
```

It writes `libg1.so`, using `mips-linux-gnu-gcc-14`, the existing `so.xn`, and
fixed `_init` address `0x51400000`. This original hack is separate from the
root CMake probe targets; installing it replaces the loaded library as before.
Do not confuse it with the smaller diagnostic `libg1_probe` package.

The rebuilt ELF has no undefined symbols; its OSTimeDly stub is exactly
`lui v1,1; ori v1,v1,16; syscall`. Compilation with extra warnings as errors
passed. A temporary host mock (without target assembly/loader sections) checked
64-file completion with 2176 one-tick delays, skipping all 64 completed files,
a short write with no completion marker, missing NAND symbol and allocation
failure. These checks do not emulate NAND, SD performance or watchdog service.
The yield-enabled full dump has not yet been console tested.

## Console result and bounded follow-up

The full yield-enabled hook completed three files and then the console reset.
Thus periodic/per-file OSTimeDly calls did not solve the observed watchdog
problem. This does not identify the expired registration, nor establish that
NAND/SD calls blocked; the loader thread may itself be preventing required
application callbacks from running.

`build.sh` now also builds **libg1-onefile.so**, with
`DUMP_FILES_PER_LOAD=1`. Install this in place of the loaded `libg1.so` using
the same replacement route. Each invocation skips accepted old completion
markers, dumps **one new 16 MiB file**, creates its marker, releases its buffer
and returns from `_init`. It does not launch a background task. Invoke again
to advance to the next incomplete file. The original full `libg1.so` remains
built with `DUMP_FILES_PER_LOAD=0` and is not claimed to avoid resets.

The one-file version isolates whether returning from the loader promptly helps.
It cannot guarantee survival of a single slow file or safe behavior of the
calling emulator after the minimal replacement module returns. If returning
causes a loader/application failure, report that behavior as well as which
`.bin` and `.ok` files were written. Both the full and one-file variants pass
host mocks for completion, skips, short data writes and allocation/symbol
failures; the bounded completion case checks exactly one new file. The new
ELF retains the expected fixed entry and has no undefined symbols.

A long-running asynchronous library worker would need a proven mechanism to
keep its code loaded and integrate with the caller's lifecycle; returning from
`_init` without that guarantee could unload code under the worker. A standalone
`.app` dumper using the proven application/message-loop lifecycle is another
candidate for finishing the whole dump in one session. Neither threaded-library
lifetime nor standalone NAND dumping has been console verified yet.
