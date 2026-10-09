#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
compiler="${CC:-mips-linux-gnu-gcc}"
app_base="${APP_BASE:-0x69800000}"
if [[ ! "$app_base" =~ ^0x[0-9a-fA-F]{8}$ ]]; then
    echo 'APP_BASE must be an eight-digit hexadecimal address' >&2
    exit 1
fi
flags=(-EL -mabi=32 -march=24kec -O2 -G0 -mno-abicalls -fno-pic
       -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables
       -fno-asynchronous-unwind-tables -std=c11 -Wall -Wextra -Werror)
"$compiler" "${flags[@]}" -c ../syscalls.c -o syscalls.o
python3 ../tools/check_syscalls.py --object syscalls.o
"$compiler" "${flags[@]}" -nostdlib -no-pie crt.c probe.c app_stubs.S syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe.app
mips-linux-gnu-readelf -h -l -S sysprobe.app > sysprobe.elfstruct
mips-linux-gnu-objdump -d sysprobe.app > sysprobe.dis
python3 check_app.py sysprobe.app
printf 'Built sysprobe.app at %s\n' "$app_base"

"$compiler" "${flags[@]}" -DPROBE_UI=1 -nostdlib -no-pie crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-ui.app
mips-linux-gnu-readelf -h -l -S sysprobe-ui.app > sysprobe-ui.elfstruct
mips-linux-gnu-objdump -d sysprobe-ui.app > sysprobe-ui.dis
python3 check_app.py sysprobe-ui.app

"$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_FONT=1 -nostdlib -no-pie crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-font.app
mips-linux-gnu-readelf -h -l -S sysprobe-font.app > sysprobe-font.elfstruct
mips-linux-gnu-objdump -d sysprobe-font.app > sysprobe-font.dis
python3 check_app.py sysprobe-font.app

index=0
for candidate in commonui fusion style apconfig all; do
    index=$((index + 1))
    output="sysprobe-default-${candidate}"
    "$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_DISCOVERY="$index" -nostdlib -no-pie \
        crt.c probe.c app_stubs.S ui_stubs.S syscalls.o -T app.xn \
        -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o "$output.app"
    mips-linux-gnu-readelf -h -l -S "$output.app" > "$output.elfstruct"
    mips-linux-gnu-objdump -d "$output.app" > "$output.dis"
    python3 check_app.py "$output.app"
done

"$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_HSV=1 -nostdlib -no-pie \
    crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S hsv.c syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-hsv.app
mips-linux-gnu-readelf -h -l -S sysprobe-hsv.app > sysprobe-hsv.elfstruct
mips-linux-gnu-objdump -d sysprobe-hsv.app > sysprobe-hsv.dis
python3 check_app.py sysprobe-hsv.app

"$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_HSV=1 -DPROBE_BITMAP=1 -nostdlib -no-pie \
    crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S hsv.c syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-bitmap.app
mips-linux-gnu-readelf -h -l -S sysprobe-bitmap.app > sysprobe-bitmap.elfstruct
mips-linux-gnu-objdump -d sysprobe-bitmap.app > sysprobe-bitmap.dis
python3 check_app.py sysprobe-bitmap.app

"$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_DODECA=1 -nostdlib -no-pie \
    crt.c probe.c app_stubs.S dodeca_demo.c dodeca_render.c ui_stubs.S hsv.c syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-dodeca.app
mips-linux-gnu-readelf -h -l -S sysprobe-dodeca.app > sysprobe-dodeca.elfstruct
mips-linux-gnu-objdump -d sysprobe-dodeca.app > sysprobe-dodeca.dis
python3 check_app.py sysprobe-dodeca.app

"$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_DODECA=1 -DPROBE_FB=1 -nostdlib -no-pie \
    crt.c probe.c app_stubs.S fb_video.c dodeca_demo.c dodeca_render.c ui_stubs.S hsv.c syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-dodeca-fb.app
mips-linux-gnu-readelf -h -l -S sysprobe-dodeca-fb.app > sysprobe-dodeca-fb.elfstruct
mips-linux-gnu-objdump -d sysprobe-dodeca-fb.app > sysprobe-dodeca-fb.dis
python3 check_app.py sysprobe-dodeca-fb.app

"$compiler" "${flags[@]}" -DPROBE_UI=1 -DPROBE_DODECA=1 -DPROBE_FB=1 -DPROBE_FAST=1 -nostdlib -no-pie \
    crt.c probe.c app_stubs.S fb_video.c dodeca_demo.c dodeca_render.c ui_stubs.S hsv.c syscalls.o \
    -T app.xn -Wl,--defsym=APP_BASE="$app_base",--build-id=none,-n -o sysprobe-dodeca-fast.app
mips-linux-gnu-readelf -h -l -S sysprobe-dodeca-fast.app > sysprobe-dodeca-fast.elfstruct
mips-linux-gnu-objdump -d sysprobe-dodeca-fast.app > sysprobe-dodeca-fast.dis
python3 check_app.py sysprobe-dodeca-fast.app
