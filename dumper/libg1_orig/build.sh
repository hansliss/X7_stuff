#!/bin/bash

mips-linux-gnu-gcc-14 -EL -o libg1.so -xc libg1.c \
    -O2 -G0 -march=24kec -mno-abicalls -fno-pic -ffreestanding \
    -nostdlib -no-pie -T so.xn \
    -Wl,--build-id=none \
    -Wl,-n,--section-start=.init=0x51400000
