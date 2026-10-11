#!/usr/bin/env python3
"""Patch local copies only; validate the known firmware and original instructions."""
import argparse
import hashlib
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]

def offset(data, address, count):
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('expected little-endian ELF32')
    if struct.unpack_from('<H', data, 18)[0] != 8:
        raise ValueError('expected MIPS')
    phoff = struct.unpack_from('<I', data, 28)[0]
    size, number = struct.unpack_from('<HH', data, 42)
    for i in range(number):
        kind, off, va, _, filesz = struct.unpack_from('<5I', data, phoff + size*i)
        if kind == 1 and va <= address and address + count <= va + filesz:
            return off + address - va
    raise ValueError(f'address {address:#x} has no file-backed LOAD segment')

def patched(data, edits):
    out = bytearray(data)
    for va, old, new in edits:
        off = offset(data, va, 4)
        actual = struct.unpack_from('<I', data, off)[0]
        if actual not in (old, new):
            raise ValueError(f'{va:#x}: expected {old:#010x}, found {actual:#010x}')
        struct.pack_into('<I', out, off, new)
        print(f'  VA {va:#010x}, file {off:#x}: {actual:#010x} -> {new:#010x}')
    return bytes(out)

USB = [(0x62c004a8, 0x04400005, 0x04000005),  # bltz zero: never skip setter
       (0x62c004bc, 0x8fa40418, 0x24040000)] # jalr delay slot: li a0,0
MSG = [(0x69406a2c, 0x71282802, 0x24050000), # li a1,0 instead of seconds*1000
       (0x6940513c, 0x8fa30430, 0x24030000)] # runtime duration updates: li v1,0

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--usb', type=Path, default=ROOT/'dumper/libg1_patch_usbapp/usb.app')
    parser.add_argument('--msg-serv', type=Path, default=ROOT/'binaries/msg_serv.app')
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parent/'patched')
    args = parser.parse_args()
    # Validate both inputs before writing anything. Default USB input already
    # includes the user's manufacturer-disk patch; unrelated bytes are preserved.
    outputs=[]
    for src, name, edits in [(args.usb,'usb.app',USB),(args.msg_serv,'msg_serv.app',MSG)]:
        dest=args.output_dir/name
        if src.resolve() == dest.resolve():
            parser.error('output must differ from input')
        data=src.read_bytes()
        print(src)
        result=patched(data,edits)
        assert patched(result,edits)==result
        changed=sum(a!=b for a,b in zip(data,result))
        outputs.append((dest,result))
        print(f'  {changed} changed bytes; sha256 {hashlib.sha256(result).hexdigest()}')
    args.output_dir.mkdir(parents=True,exist_ok=True)
    for dest,data in outputs:
        if dest.exists() and dest.read_bytes()!=data:
            parser.error(f'refusing to overwrite different output: {dest}')
    for dest,data in outputs:
        dest.write_bytes(data)
        print(f'wrote {dest}')

if __name__ == '__main__':
    main()
