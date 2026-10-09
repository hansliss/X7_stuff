# Limited syscall probe

`libg1.so` is the ready-built filesystem probe. Install it through the same
mechanism as the existing dumper modules, under the name `libg1.so`. It runs once
from `_init`, writes `/mnt/card/sysprobe-fs.txt`, and returns. It does not run an
emulator or wait for input. Start with this variant.

All variants are little-endian MIPS O32 executables with entry point
`0x51400000`, two load segments, and the existing dumper's `G1_SO_VERSION`
export convention. They use the root `syscalls.c` wrappers and require no libc.
All four variants have run successfully on the console. The v3 directory loop
returned `"."`, `".."`, `"entry.bin"`, and then end-of-directory; see
[results](../logs/sysprobe-results.md).

## Variants

Copy one variant at a time to your installation location as `libg1.so`:

| Build | SD card log | What it checks |
| --- | --- | --- |
| `libg1-fs.so` (also `libg1.so`) | `sysprobe-fs.txt` | 32-byte write/read, tell, absolute and negative relative/end seeks, invalid-descriptor 64-bit return, fsync, ftruncate, truncate, rename, remove |
| `libg1-metadata.so` | `sysprobe-metadata.txt` | fstat, stat, statfs into guarded buffers; saves raw output for structure analysis |
| `libg1-directory.so` | `sysprobe-directory.txt` | mkdir, directory open, tell, readdir, seekdir, rewinddir, rmdir |
| `libg1-watchdog.so` | `sysprobe-watchdog.txt` | Opens its own watchdog descriptor, sets a 2000 ms budget, refreshes four times, closes |

Run metadata and directory variants after the filesystem probe works. Their
output layouts remain provisional. Each uses an aligned 4096-byte buffer with
64-byte guards on either side. Guards detect an overwrite; they cannot prevent
one. Metadata saves `sysprobe-fstat.bin`, `sysprobe-stat.bin`, and
`sysprobe-statfs.bin`; directory saves `sysprobe-readdir.bin`. Dumps contain all
4096 bytes, initially filled with `a5`. The log gives changed-byte counts and the
last changed byte plus one; bytes written as `a5` are invisible to that comparison.
Directory readdir now starts at position zero with capacity 1024, matching LIBC_FS.SO.
It follows returned cursors for at most four calls and saves additional outputs
as `sysprobe-readdir-1.bin` through `sysprobe-readdir-3.bin`.
The initial count-one probe returned zero without touching its buffer; analysis
of FS.KO supports interpreting this argument as buffer capacity in bytes.

Logs and dump files are overwritten on repeated runs. Scratch files are
`/mnt/card/sysprobe-work.bin`, `/mnt/card/sysprobe-renamed.bin`, and
`/mnt/card/sysprobe-dir/entry.bin`. The probe checks for existing scratch paths
and skips rather than overwriting them. Creation requires an open result of
`-2` (presumed ENOENT); other errors also cause a skip. It removes scratch it
created when execution reaches cleanup. An interrupted run can leave scratch
behind; remove those dedicated scratch paths before retrying, after checking
they belong to the probe. Other SD card contents are not used.

## Reading a log

Values are 16 hexadecimal digits. Signed errors use two's complement: `-9`, for
example, is `fffffffffffffff7`. Filesystem assertions log the observed value,
`EXPECTED`, and `PASS` or `FAIL`. The invalid-descriptor tell check tests whether
the firmware sign-extends its error across the full 64-bit return value.

Every `BEFORE` marker is written and fsynced before yielding one OS tick and
starting the named call. After a reset, the last marker identifies the next call
or the intervening yield as a possible stopping point; it does not prove which
one failed. `DONE` means control flow completed, including any skips. Zero
assertion failures does not establish success for observational calls such as
stat/readdir: inspect their raw status and dumps too. Logging failure stops
further experiments. No log can mean the module was not invoked or its initial
log open failed.

Work and retry loops are bounded; individual firmware calls or SD card I/O can
still block. There are no large-file writes, intentional timeout experiments,
scheduler locks, or long sleeps. One-tick yields between checkpoints allow
other tasks to run, but cannot guarantee that the console's watchdog machinery
continues servicing its registrations while this loader callback runs.

## Watchdog findings

The clearest watchdog code is in `binaries/WATCHDOG.KO` and `manager.app`, rather
than `LIBG1.SO`:

* `WATCHDOG.KO` registers device name `wd` (major 250). Its open routine at
  `0xca0006d0` creates a separate timeout record for each descriptor.
* The ioctl routine at `0xca000870` accepts command **0** and a numeric third
  argument in milliseconds, **not a pointer**. It sets that descriptor's budget
  and clears its elapsed counter. A zero budget is ignored by the timeout check.
* The timer/check routines at `0xca0002fc` / `0xca000120` check registrations
  every 500 ms and feed the hardware watchdog. Expiration of any active
  registration enters the reboot path. Closing removes that descriptor's record.
* `manager.app` `_start_watchdog` at `0x60000070` opens `/dev/wd`, installs a
  timer callback at half its timeout, and calls ioctl command zero. Its default
  `watchdog_time` at `0x60003028` is **5000 ms**; runtime configuration may change
  it. The callback at `0x6000014c` calls `check_soft_watchdogs()` and refreshes
  the manager's descriptor. Application software watchdogs are separate.

Feeding a newly opened descriptor therefore does **not** refresh the manager's
existing descriptor or an application's software watchdog. The optional
watchdog variant demonstrates the driver interface, not a solution to long
running probes. Its 2000 ms budget could expire if SD logging stalls; use it
only when the simpler probe is working. It never intentionally disables the
watchdog or waits for a timeout.

In `LIBG1.SO`, calls at `0x5143af60`, `0x51470510`, and `0x51470524` invoke the
group-7 `usleep` stub (`0x70084`) with 60000 or 50000 microseconds. These provide
a plausible scheduling opportunity; I did not identify a direct watchdog feed
there. `camera.app` also exports a `sys_forbid_soft_watchdog` stub (`0x1200b0`),
but its arguments and implementation have not been established. The probe does
not call it.

## Rebuilding

Configure this package independently from the workspace root:

```sh
cmake -S libg1_probe -B build-libg1
cmake --build build-libg1 --parallel
```

Outputs are `build-libg1/libg1.so` and the four `libg1-*.so` variants.
Add `-DPROBE_YIELD=OFF` at configure time to omit checkpoint yields.
The MIPS cross toolchain is selected automatically; CMake 3.20+ is required.
The shared syscall sources and validation inputs stay at the workspace root.
See [workspace build instructions](../README.md) for building both probes.

The original shell build remains available and writes outputs here:

From the repository root:

```sh
libg1_probe/build.sh
```

`CC` defaults to `mips-linux-gnu-gcc`. The build treats warnings as errors and
checks all 243 syscall wrapper instruction sequences. It also produces
`.dis` and `.elfstruct` inspection files for each variant.

For an explicit comparison without checkpoint yields:

```sh
PROBE_YIELD=0 libg1_probe/build.sh
```

This replaces all built variants. The watchdog variant still has its explicit
one-tick delay. Rebuild normally to restore the default. Send back the relevant
`sysprobe-*.txt` logs and, for metadata/directory runs, the raw `.bin` dumps to
refine the syscall declarations.
