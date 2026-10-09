#!/usr/bin/env python3
"""Check the recovered X7 ABI map; optionally check compiled stub opcodes.

No third-party Python modules required. Scan vendor ELF executable sections for
lui/ori/syscall stubs and record their addresses as reproducible caller evidence.
"""
import argparse
import collections
import hashlib
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
BASE = 0xc0000000
TABLES = {'SYSCALL': (0x40998, 130, 0x10000),
          'KMODULE': (0x40450, 113, 0x20000)}


def sections(data):
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected ELF32 little endian')
    offset = struct.unpack_from('<I', data, 32)[0]
    entry_size, count = struct.unpack_from('<HH', data, 46)
    assert entry_size == 40
    return [struct.unpack_from('<10I', data, offset + i * 40)
            for i in range(count)]


def recover(data):
    exports = collections.defaultdict(list)
    for offset in range(0x47c58, 0x47c58 + 227 * 8, 8):
        address, pointer = struct.unpack_from('<II', data, offset)
        assert 0x47000 <= pointer - BASE < 0x47c58
        name = data[pointer - BASE:].split(b'\0', 1)[0].decode('ascii')
        exports[address].append(name)
    rows = []
    for group, (offset, count, api_base) in TABLES.items():
        for index in range(count):
            address = struct.unpack_from('<I', data, offset + index * 4)[0]
            assert BASE <= address < BASE + 0x40000
            rows.append((group, index, address, exports.get(address, [])))
        # Both arrays end immediately before ASCII strings, not another pointer.
        after = struct.unpack_from('<I', data, offset + count * 4)[0]
        assert not BASE <= after < BASE + 0x40000
    anchors = {('SYSCALL', 68): 'sys_open', ('SYSCALL', 72): 'sys_lseek',
               ('KMODULE', 35): 'kmalloc', ('KMODULE', 62): 'kernel_sym'}
    for group, index, _, names in rows:
        if (group, index) in anchors:
            assert anchors[group, index] in names
    return rows


def binary_stubs(directory):
    result = collections.defaultdict(list)
    for path in sorted(directory.iterdir()):
        if not path.is_file():
            continue
        data = path.read_bytes()
        if data[:4] != b'\x7fELF':
            continue
        for _, kind, flags, address, offset, size, *_ in sections(data):
            if kind != 1 or not flags & 4:
                continue
            for pos in range(0, size - 11, 4):
                hi, lo, trap = struct.unpack_from('<III', data, offset + pos)
                if hi not in (0x3c030001, 0x3c030002) or trap != 12:
                    continue
                if lo >> 16 not in (0x3463, 0x2463):
                    continue
                api = ((hi & 0xffff) << 16) + (lo & 0xffff)
                result[api].append(f'{path.name}@0x{address + pos:08x}')
    return result


def check_object(path, rows, symbols):
    data = path.read_bytes()
    sh = sections(data)
    found = {}
    for _, kind, _, _, offset, size, link, _, _, entry_size in sh:
        if kind != 2:
            continue
        _, _, _, _, str_offset, str_size, *_ = sh[link]
        strings = data[str_offset:str_offset + str_size]
        for pos in range(offset, offset + size, entry_size):
            name, value, length, info, _, section = struct.unpack_from('<IIIBBH', data, pos)
            if info != 0x12:  # STB_GLOBAL | STT_FUNC
                continue
            symbol = strings[name:].split(b'\0', 1)[0].decode()
            assert length == 12, (symbol, length)
            code_offset = sh[section][4] + value
            found[symbol] = struct.unpack_from('<III', data, code_offset)
    assert set(found) == set(symbols.values()), 'Compiled symbol coverage differs'
    for group, index, _, _ in rows:
        symbol = symbols[group, index]
        api_base = TABLES[group][2]
        assert found[symbol] == (0x3c030000 | (api_base >> 16),
                                 0x34630000 | index, 12), symbol
    print(f'Checked all {len(found)} compiled functions: exact 12-byte stubs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--object', type=Path)
    parser.add_argument('--report', type=Path, help='Write TSV including vendor stub evidence')
    args = parser.parse_args()
    data = (ROOT / 'SYSCFG.SYS').read_bytes()
    rows = recover(data)
    source = (ROOT / 'syscalls.c').read_text()
    matches = re.findall(r'^X7_API\((\w+), (SYSCALL|KMODULE)_API_START, (\d+)\); /\* 0x([0-9a-f]+): (\S+) \*/$', source, re.M)
    symbols = {(group, int(index)): symbol for symbol, group, index, _, _ in matches}
    assert len(matches) == len(symbols) == len(rows)
    declarations = (ROOT / 'syscalls.h').read_text()
    for group, index, address, names in rows:
        symbol = symbols[group, index]
        expected_name = names[0] if names else '-'
        assert (symbol, group, str(index), f'{address:08x}', expected_name) in matches
        assert re.search(r'\b' + re.escape(symbol) + r'\([^;]*\);', declarations)
        assert f'#define X7_API_{symbol} ({group}_API_START + {index})' in declarations
    print(f'Checked {len(rows)} slots and declarations; SYSCFG.SYS sha256={hashlib.sha256(data).hexdigest()}')
    if args.object:
        check_object(args.object, rows, symbols)
    if args.report:
        usage = binary_stubs(ROOT / 'binaries')
        out = ['group\tindex\tapi\taddress\texport\twrapper\tvendor_stubs']
        for group, index, address, names in rows:
            api = TABLES[group][2] + index
            out.append('\t'.join([group, str(index), f'0x{api:05x}', f'0x{address:08x}',
                                   '/'.join(names) or '-', symbols[group, index],
                                   ', '.join(usage.get(api, []))]))
        args.report.write_text('\n'.join(out) + '\n')
        known = {TABLES[g][2] + i for g, i, _, _ in rows}
        print(f'Wrote report: {len(known & usage.keys())} slots have vendor ELF stub evidence')


if __name__ == '__main__':
    main()
