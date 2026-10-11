# Force the backlight/screensaver duration to zero

The script generates local patched copies, not an installed firmware change.
Generated `patched/` apps are no longer bundled; regenerate them with the
command below. Original firmware inputs and the manufacturer-disk patch remain
preserved because the script requires them. No console
files or original binaries were overwritten. The default USB input is the
existing `../libg1_patch_usbapp/usb.app`, which already has the manufacturer-disk
patch. That patch is preserved.

## Which app actually controls it?

`usb.app` reads `BACKLIGHT_DURATION` at `0x62c00494`, then calls the charging
scene's `backup_idle_time` function. The function pointer at offset 12 of the
scene API is `0x62c01d44`: it stores the value in `s_idle_time_backup`, not in
the system's persistent configuration. USB charging has its own timeout logic
and restores the saved system timeout. Thus patching this reader alone is
insufficient for normal launcher/game inactivity behavior.

`msg_serv.app` is the system message/event service: it handles keys, LCD,
power events and timers. Its configuration table contains `BACKLIGHT_DURATION`
at `0x6940a884`, with the value at `0x6940a8ac`. `load_sys_timers_config` uses
this value for system timer index 0, whose callback is
`_scrsaver_timeout_proc`. `start_sys_timer` explicitly skips creating/restarting
a timer when its duration is zero (`0x69406aec`). Runtime duration changes also
come through a system message at `0x6940512c`.

The supplied msg_serv patch forces zero both when loading configuration and
when processing the duration-change message. It therefore enforces the zero
*effective timeout* across boots/reloads and subsequent normal API updates.
It does not repair configuration persistence, rewrite the on-disk settings,
or promise that a settings UI will always display zero. Other idle/sleep and
shutdown timers are left alone. A direct out-of-band timer modification outside
these inspected paths is not covered.

## Exact patches for these firmware images

MIPS is little endian here. A virtual instruction address is not a file offset:
the script maps through the ELF LOAD segments.

| Image | Virtual address | File offset | Original bytes | Replacement bytes | Meaning |
| --- | --- | --- | --- | --- | --- |
| usb.app | `0x62c004a8` | `0x528` | `05 00 40 04` | `05 00 00 04` | `bltz zero,...` is never taken; always reach the backup setter |
| usb.app | `0x62c004bc` | `0x53c` | `18 04 a4 8f` | `00 00 04 24` | `li a0,0` in the indirect call's delay slot |
| msg_serv.app | `0x69406a2c` | `0x6aac` | `02 28 28 71` | `00 00 05 24` | `li a1,0` instead of multiplying configured seconds by 1000 |
| msg_serv.app | `0x6940513c` | `0x51bc` | `30 04 a3 8f` | `00 00 03 24` | `li v1,0` instead of loading a requested runtime duration |

The minimal USB-only patch is the four-byte instruction replacement at
`0x53c` if get_config always succeeds. The additional one-byte change at
`0x52a` makes it also work when that lookup fails. The final USB patch changes
five bytes, in two instruction slots. The msg_serv patch changes eight bytes
in two instruction slots; omitting its runtime-update patch would allow a
later settings/API call to enable the timeout again.

MIPS executes the instruction immediately after `jalr` before entering the
callee. That is why setting `a0` at `0x62c004bc` changes the argument to zero.
It is not necessary to replace the entire lookup block or add a code cave.

## Generate and validate

```
python3 dumper/backlight_patch/patch.py
```

Outputs are `patched/usb.app` and `patched/msg_serv.app`. Use `--usb` or
`--msg-serv` to select another local input, and `--output-dir` for another
output directory. The script checks original/replacement instructions, leaves
other bytes unchanged, is idempotent, rejects in-place output and refuses to
overwrite differing existing outputs. It is intended for the supplied firmware
revision; matching instruction bytes alone does not establish compatibility
with another revision.

The default USB input differs from `binaries/usb.app` at only file offset
`0x48c` (1 -> 0), the existing manufacturer-disk change. Patched copies retain
ELF sections, symbols, load addresses and file sizes. Both were disassembled to
verify the intended instructions. The user subsequently reported that the fix
worked. The installed image combination was not recorded, so this does not
establish separate hardware coverage of every USB charging and service path.
Keep the original files for restoration; msg_serv is a boot-time system service,
so a hardware test requires a known way to restore its original disk file.

## Rebuild versus binary patch

A full decompiled/recompiled usb.app is not preferable for these changes.
The ELF retains function names and some debugging information, which helps
analysis, but it is not complete C source. It includes USB mass-storage/MTP,
charging, GUI and runtime code, with fixed addresses and vendor ABIs. A faithful
rebuild would require recovering these interfaces, data layouts, compiler
assumptions and library dependencies, then testing each mode. Merely compiling
a decompiler's output cannot establish equivalence.

Keep the original executable and use checked instruction patches for now.
The patch script is a reproducible setup for that workflow. If substantially
larger USB changes become necessary, selectively reconstruct functions and
redirect calls to a separately built payload before attempting a full app
replacement. No decompiled build is needed for the duration fix.
