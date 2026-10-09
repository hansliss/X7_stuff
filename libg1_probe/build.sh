#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
compiler="${CC:-mips-linux-gnu-gcc}"
yield_enabled="${PROBE_YIELD:-1}"
if [[ "$yield_enabled" != 0 && "$yield_enabled" != 1 ]]; then
    echo "PROBE_YIELD must be 0 or 1" >&2
    exit 1
fi
flags=(-EL -mabi=32 -march=24kec -O2 -G0 -mno-abicalls -fno-pic
       -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables
       -fno-asynchronous-unwind-tables -std=c11 -Wall -Wextra -Werror)
"$compiler" "${flags[@]}" -c ../syscalls.c -o syscalls.o
python3 ../tools/check_syscalls.py --object syscalls.o
for mode in 0 1 2 3; do
    case "$mode" in
        0) variant=fs;; 1) variant=metadata;; 2) variant=directory;; 3) variant=watchdog;;
    esac
    "$compiler" "${flags[@]}" -DPROBE_MODE="$mode" -DPROBE_YIELD="$yield_enabled" \
        -nostdlib -no-pie libg1.c syscalls.o -T so.xn \
        -Wl,--build-id=none,-n,--section-start=.init=0x51400000 \
        -o "libg1-$variant.so"
    mips-linux-gnu-readelf -h -l -S "libg1-$variant.so" > "libg1-$variant.elfstruct"
    mips-linux-gnu-objdump -d "libg1-$variant.so" > "libg1-$variant.dis"
done
cp -- libg1-fs.so libg1.so
printf 'Built libg1.so (filesystem), plus metadata, directory, and watchdog variants.\n'
