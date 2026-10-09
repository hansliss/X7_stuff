# X7 firmware syscall wrappers

`syscalls.c` and `syscalls.h` cover every slot in both dispatch arrays in the
supplied `SYSCFG.SYS`: 130 SYSCALL slots and 113 KMODULE slots. This includes
unnamed functions, duplicate targets, and reserved no-ops. Numbers come from the
actual pointer arrays, not the order of exported names. The firmware's export
pool starts at file offset `0x47000`; 227 `{address, name_pointer}` pairs start at
`0x47c58`. Runtime pointers map to file offsets by subtracting `0xc0000000`.

| Group | API base | Array file offset | Slots | First byte after array |
| --- | --- | --- | --- | --- |
| SYSCALL | `0x10000` | `0x40998` | 0–129 | `0x40ba0` (`APP_vMain`) |
| KMODULE | `0x20000` | `0x40450` | 0–112 | `0x40614` (`sys_malloc`) |

The supplied image's SHA256 is
`91e7cfc6927e224e607ba3c1bc9cabc0305f98d18db461f952db91dd3a58e1d8`.
The earlier conversation enumerated the SYSCALL table only through slot 111;
there are another 18 entries. The two discussion documents supply the hardware,
loader, and on-device test context. The syscall discussion and the working
64-bit seek experiment are in `Running MAME Games.md`, turns 43–45; the other
document concerns emulator ROM compatibility and supplies no additional ABI.

## Related recovered interfaces

These two tables do not include the library GUI/application API groups.
See [GUI, fonts and buttons](sysprobe/GUI.md) for group 3/group 18 stubs,
[framebuffer/video ABI](sysprobe/FRAMEBUFFER.md) for `/dev/fb` commands and
console-verified uses of group-1 slots 114–116, and
[rendering experiments](sysprobe/DEMOS.md) for results and timing limits.
The chronological screen/video evidence is in [UI-notes.md](sysprobe/UI-notes.md).

## Using the files

The supplied CMake setup builds and links `syscalls.c` automatically; no manual
object build is needed for either probe package. In
[cmake/X7Probe.cmake](cmake/X7Probe.cmake), `x7_syscalls` is an OBJECT library,
and `x7_probe_target()` links it into each probe and depends on
`x7_check_syscalls`, which validates all 243 stubs. CMake stores the object in
its build tree as `CMakeFiles/x7_syscalls.dir/syscalls.c.obj` (relative to the
root or standalone package build directory), rather than a source-directory
`syscalls.o`. You can build just the object and validation target with:

```sh
cmake --build build --target x7_check_syscalls
```

The package `build.sh` scripts also compile and validate the wrappers
automatically, using `syscalls.o` in their package directories.

For integrating the wrappers into another module outside this build setup,
the manual command below is still valid. Build `syscalls.c` separately and
link that object alongside your module. Remove any old definitions of the same
wrappers from the module source.

```sh
mips-linux-gnu-gcc -EL -mabi=32 -march=24kec -G0 -mno-abicalls \
  -fno-pic -ffreestanding -fno-builtin -std=c11 -Wall -Wextra -Werror \
  -c syscalls.c -o /tmp/x7-syscalls.o
python3 tools/check_syscalls.py --object /tmp/x7-syscalls.o
```

Use the existing dumper linker script and loader entry/export sections. Retain
`--build-id=none` when linking that high-address module, as established in the
conversation. These files do not supply `_init`, `_fini`, or module exports.

```c
#include "syscalls.h"

int fd = sys_open("/mnt/card/example.bin", X7_O_RDWR, 0666);
if (fd >= 0) {
    x7_off_t position = sys_lseek(fd, (x7_off_t)0x423d, X7_SEEK_SET);
    /* Check position, then read/write as needed. */
    (void)position;
    sys_close(fd);
}
```

Types have `x7_` prefixes so the compiler's libc cannot silently substitute its
own `off_t`, `size_t`, or structures. Standard libc-like functions also have
`x7_` prefixes (`x7_malloc`, `x7_memcpy`, `x7_printf`, `x7_dlopen`, etc.). Firmware
names such as `sys_open`, `OSSemCreate`, `kmalloc`, and `kernel_sym` are retained.
SYSCALL's duplicate allocation entries are `x7_sys_kmalloc`/`x7_sys_kfree`;
`kmalloc`/`kfree` use the original dumper's KMODULE numbers. Unnamed entries use
`x7_syscall_NNN` or `x7_kmodule_NNN`, with decimal indices. Every wrapper also
has a full API-number constant named `X7_API_<wrapper>`.

These are little-endian MIPS32 O32 traps, not Linux syscalls. The number goes in
`v1`; the trap instruction has code zero. Vendor stubs in `LIBC_FS.SO`,
`LIBC_SYS.KO`, `FS.KO`, and the other supplied ELF binaries have exactly
`lui v1; ori v1; syscall`, just like `libg1.c`'s expanded `li; syscall`.
There is deliberately no normal return instruction after the trap. Stubs do
not create a stack frame, preserving the caller's stack arguments. Assembly
settings are pushed/popped so they do not affect surrounding compiler output.

## Confidence and unresolved structures

Each declaration has its table index, target address, export name, and a label:

- **E:** register/stack use, width, or implementation directly supports the ABI.
  Parameter names, signedness where indistinguishable, and pointed-to types can
  still be interpretations. E does not mean every semantic detail was verified.
- **I:** a best-effort signature inferred from the implementation, function
  family, or familiar uC/OS and kernel interfaces. Callback signatures and
  vendor-specific semantics need further checking before use.
- **R:** the ABI remains unresolved. Four slots retain eight explicit raw words
  in `a0..a3` and caller stack offsets 16, 20, 24, 28. This is an investigation
  interface, not a claim that the function takes eight arguments. It exposes
  only `v0`; callers must account for 64-bit alignment and return values.

Reserved no-ops have `void (void)` declarations because they do not initialize
`v0`; assigning them an integer result would expose an undefined register value.
SYSCALL slots 42–49 all point to `0xc0011e40`; KMODULE slots 28–31 and 110–112 all
point to `0xc000b58c`.

OS event/TCB/query structures, driver operations, timers, and work items remain
opaque and must not be replaced with host/Linux libc structures. The console
probe dumps and FS.KO methods now support SD/FAT `x7_stat` and `x7_statfs`
layouts in the header; unresolved fields retain offset-based names. Other
filesystems have not been tested. See [console probe results](logs/sysprobe-results.md).
Directory calls retain a `void *` buffer; `x7_dirent` describes the recovered
variable-length SD/FAT header, with unresolved fields left explicit. Passing an undersized output object is not safe.

## ABI findings that affect callers

### 64-bit file offsets

`x7_off_t` is **signed 64-bit**, explicitly `long long`. On O32, both `int` and
`long` are 32-bit. The little-endian seek call is:

| Argument location | Meaning |
| --- | --- |
| `a0` | fd |
| `a1` | alignment hole |
| `a2` | low 32 bits of offset |
| `a3` | high 32 bits of offset |
| caller `sp+16` | whence |
| returned `v0`, `v1` | low/high 32 bits of result |

`sys_lseek` at `0xc001cd34` saves `a2/a3` and reads `80(sp)` after allocating 64
bytes, i.e. caller `sp+16`. Its epilogue explicitly returns both words. This
matches the user's successful on-device correction to `long long`.

`sys_tell` at `0xc001ccf0` also returns `v0/v1`. `sys_truncate` at `0xc001c278`
and `sys_ftruncate` at `0xc001c398` save `a2/a3` for the aligned length argument;
their result is a 32-bit status.

`sys_mmap` also needs a **64-bit final offset argument**, even though its
implementation uses only the low word: it reads fd from caller `sp+16` and
offset from caller `sp+24` (`88(sp)` and `96(sp)` after a 72-byte frame).
A 32-bit offset prototype would put the value at `sp+20` and break the call.
The high word's semantics are unverified; the implementation adds the low word
to the mapping address. `sys_munmap` and `sys_msync` are zero-returning stubs in
this image, and `fork` simply returns zero; they do not implement the usual
POSIX behavior.

### Directory calls

`LIBC_FS.SO` calls its SYSCALL[91] stub at `0x40403bdc` from `0x404001f0` with
fd in `a0`, a stored 64-bit position in `a2/a3`, buffer at `sp+16`, and a count
at `sp+20`. The count comes from a field initialized to 1024. The corresponding
reverse-direction call to SYSCALL[95] at `0x40400450` uses the same arrangement
and subtracts 32 from the 64-bit position. These support:

```c
int sys_readdir(int fd, x7_off_t position, void *entries, x7_u32 count);
int sys_prevdir(int fd, x7_off_t position, void *entries, x7_u32 count);
int sys_seekdir(int fd, x7_off_t position);
```

The kernel forwards `a2/a3` and both stack words to filesystem methods.
`sys_seekdir` forwards `a2/a3`; `sys_rewinddir`, `sys_lastdir`, and
`sys_reset2parentdir` take a descriptor. Console v2 confirms that readdir's count is buffer capacity in bytes and its
positive result counts records. A 1024-byte buffer returned one record for
`"."`; capacity one returned zero and wrote nothing. The recovered header has a
32-bit next-position cursor at +4, timestamp at +8, 16-bit record length at +12,
FAT attributes at +14, and NUL-terminated name at +16. Advance by the record
length, not `sizeof(struct x7_dirent)`. Fields +0 and +15 are unresolved and
were untouched. The v3 console run verifies the same layout for `".."` and `"entry.bin"`,
including next cursors 64/96 and file archive attribute 0x20. A subsequent call
at cursor 96 returns zero with untouched output. Too-small capacity can also
return zero, so this is only EOF evidence with adequate space. Prevdir output
remains unverified; do not
assume a POSIX `DIR *` or `struct dirent`.

### Other departures and useful details

- `sys_getcwd` returns a **32-bit status**, not a character pointer: the error
  path at `0xc0018138` returns `-22`, and success forwards a filesystem method's
  status. The output buffer is its first argument.
- `sys_mount` really has five words: source, target, filesystem, flags, data.
  It forwards caller `sp+16` from `0xc001c1f8`. `sys_umount` uses a path.
- `OSQPostByPrio` takes a fourth, 16-bit **option** argument in `a3`, in
  addition to event, message, and 8-bit priority (`0xc0014314..0xc0014318`).
- `OSTimeDly` masks the tick count to 16 bits. `OSTaskCreateExt` has the familiar
  nine logical arguments, with id and option explicitly loaded as 16-bit
  values. Slot 5 is a different task-creation implementation, not an alias.
- `request_act213x_dma` has **five arguments**. It forwards handler from `a2`,
  flags from `a3`, name from `a1`, and device from caller `sp+16` to
  `request_irq` (`0xc0003ddc..0xc0003df4`). `get_dma_chan` returns a pointer to
  a 24-byte channel record, not a channel number.
- `register_blkdev` consumes major and name; it does not consume an operations
  pointer. `blk_init_queue` consumes one callback, with no lock argument.
  `blk_read`/`blk_write` have five argument words (inode, file, buffer, count,
  position pointer), and `blk_ioctl` has four. Their types are firmware-specific.
- `get_base_name` takes path and an output buffer and returns status. It copies
  a prefix derived by `get_rel_name`, so its name does not guarantee POSIX
  basename behavior. No destination capacity argument is checked.
- Despite its name, `module_load` at `0xc000f308` queries a task/address
  derived from its second word and returns a boolean; it does not load a
  module file. `reset_baudrate` consumes no argument and returns 1.
  `set_image_path_name_of_pc` takes an image selector before the path;
  `set_uview_mode` consumes two words.
- `get_sysmsg` takes no argument and returns a message word or a negative
  error. `put_sysmsg` takes one word. `get_regs(selector)` reads a performance
  control register when selector is zero; the export name is misleading.
  `get_pcnt` and `get_ab_ticks` return **64-bit** values.
- `kmalloc` accepts the established `(bytes, flags)` calling convention, but
  this particular implementation overwrites `a1` with the caller's return
  address and passes it to its internal allocator. The supplied flags are
  ignored here. `kfree` similarly records the caller.
- `insmod` forwards a second argument to the module loader; its meaning is
  represented as opaque `module_information`. `_execve` forwards two words to
  the loaded entry function; argc/argv versus another vendor convention is not
  resolved. `_exit` returns and appears to use a task/address identifier, so
  the wrapper is not declared `noreturn` or given a POSIX status parameter.
- Raw calls return firmware values directly. For filesystem calls, negative
  errors such as `-9`, `-20`, and `-22` are visible in the implementation.
  Wrappers do not convert them to `-1` or set libc `errno`. Pointer-valued APIs
  can also return encoded negative errors (e.g. `kernel_sym` returns `-22` for
  an invalid name); checking only for NULL may be insufficient.

## Unnamed entries

Numerical wrapper names are retained even where the instruction sequence gives
a strong clue, so inferred historical SDK names are not presented as exports.

| Slots | Observation / working interpretation |
| --- | --- |
| S5 | Extended task creation; stack arguments and vendor additions remain unresolved (R). |
| S7 | Priority plus packed task/address identifier; related to task deletion (E for word placement). |
| S112 | One input is visibly consumed, but time/delay semantics unresolved (R). |
| S113 | Returns pointer `0xc0049660`, no arguments (E). |
| S114–116 | Allocation by size/flags, release by pointer, and address conversion (I/I/E). |
| S117 | Tail-jumps to `os_mem_query`, returns available bytes (E). |
| S118–119 | Setjmp/longjmp-like register save/restore, 48-byte context; retain raw ABI (R). These need trap-context and compiler `returns_twice` investigation before normal C use. |
| S120–122 | No-argument byte getters; the last extracts bits 20–27 from a global word (E). |
| S123–124 | Set per-task memory metadata / clear the metadata table (I). |
| S125–126 | Operation plus argument / non-null pointer plus byte count; semantics unresolved (I). |
| S127 | Writes one character through the serial-output helper (I). |
| S128–129 | Hardware nibble getter / selector-based hardware query (E/I). |
| K61 | Releases a file-like object and bookkeeping (I). |
| K96–97 | Installs a hook / retrieves the current task's hook pointer (I/E). |
| K98–100 | Printf-like variadic output / output-mode getter / callback installation (E/E/I). |
| K102–103 | Address plus length cache maintenance; exact cache operation still inferred (I). |

S means SYSCALL; K means KMODULE. All addresses and vendor stub occurrences
are recorded in `syscalls.tsv`. A vendor stub proves the API number was used,
not that the caller's full prototype or every branch is understood.

## Validation

The complete files compile with the installed `mips-linux-gnu-gcc` using the
flags above and `-Werror`. `tools/check_syscalls.py` independently re-parses
both tables, matches all 243 wrappers and header declarations, and checks the
compiled ELF function symbols and **all three machine words of every stub**.
Scanning executable sections of the supplied vendor ELFs found matching stubs
for 173 of the recovered slots. Regenerate that evidence with:

```sh
python3 tools/check_syscalls.py --object /tmp/x7-syscalls.o --report syscalls.tsv
```

Separately compiled C call probes with nonzero high/low offset words confirmed
that the header generates the required `a2/a3` pairs for seek/truncate/directory
calls, whence at `sp+16`, directory buffer/count at `sp+16/+20`, and mmap's
64-bit offset at `sp+24/+28`. These are compile/disassembly checks; no new calls
were executed on the handheld, and inferred signatures remain provisional.

A temporary copy of the existing USB-patch module was also linked against the
new object using the original loader script. It has no undefined symbols,
entry point `0x51400000`, and the expected two LOAD segments (text at
`0x51400000`, data at `0x51401000`). The original dumper source and binaries
were left intact.
