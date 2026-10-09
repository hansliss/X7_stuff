#!/usr/bin/env python3
"""Check the custom ELF and compare runtime stubs with the vendor app."""
import pathlib
import struct
import sys


def read_elf(path):
    b = pathlib.Path(path).read_bytes()
    assert b[:7] == b'\x7fELF\x01\x01\x01', path
    h = struct.unpack_from('<HHIIIIIHHHHHH', b, 16)
    assert h[0:2] == (2, 8), 'Expected MIPS ET_EXEC'
    assert h[6] & 0x70001000 == 0x70001000, 'Expected MIPS32r2 O32'
    segments = [struct.unpack_from('<8I', b, h[4] + i*h[8])
                for i in range(h[9])]
    sections = [struct.unpack_from('<10I', b, h[5] + i*h[10])
                for i in range(h[11])]
    symbols = {}
    for s in sections:
        if s[1] != 2:
            continue
        strings = sections[s[6]]
        names = b[strings[4]:strings[4]+strings[5]]
        for off in range(s[4], s[4]+s[5], s[9]):
            name, addr, size, info, other, section = struct.unpack_from('<IIIBBH', b, off)
            label = names[name:names.find(b'\0', name)].decode()
            assert not label or section, f'Undefined symbol {label}'
            if label:
                symbols[label] = (addr, size)
    return b, h, segments, symbols


def at(b, segments, addr, size):
    for typ, off, va, pa, filesz, memsz, flags, align in segments:
        if typ == 1 and va <= addr and addr+size <= va+filesz:
            return b[off+addr-va:off+addr-va+size]
    raise AssertionError(f'Address 0x{addr:x} not file-backed')


b, h, segments, symbols = read_elf(sys.argv[1])
loads = [s for s in segments if s[0] == 1]
assert len(loads) == 2 and [s[6] for s in loads] == [5, 6]
assert h[3] == symbols['__start'][0] and h[3] != symbols['_init'][0]
assert symbols['_init'][0] == loads[0][2]
assert loads[1][5] > loads[1][4], 'Expected zero-initialized BSS'
assert 0x60000000 <= loads[0][2] < loads[1][2]+loads[1][5] < 0x80000000
for s in loads:
    assert s[2] == s[3] and s[4] <= s[5]

root = pathlib.Path(__file__).resolve().parent.parent
vendor = root / 'binaries/calculat.app'
vb, vh, vs, vsyms = read_elf(vendor)
mapping = {
    'app_get_pid': ('__kernel_get_pid', 4),
    'app_release_pid': ('__kernel_release_pid', 5),
    'app_get_process': ('__kernel_get_process_struct', 6),
    'app_change_thread_pid': ('_kernel_change_thread_pid', 14),
    'app_getpid': ('getpid', 22),
    'app_set_envp': ('__kernel_set_envp', 29),
    'app_current_pid': ('__current_pid', 37),
    'app_insert_child': ('__kernel_insert_sub_to_parent', 39),
    'app_remove_child': ('__kernel_remove_sub_from_parent', 40),
    'app_thread_create': ('pthread_create', 96),
    'app_attr_init': ('pthread_attr_init', 112),
    'app_attr_destroy': ('pthread_attr_destroy', 113),
    'app_attr_inheritsched': ('pthread_attr_setinheritsched', 114),
    'app_attr_detachstate': ('pthread_attr_setdetachstate', 116),
    'app_exit': ('exit', 130),
}
for name, (vname, index) in mapping.items():
    addr, size = symbols[name]
    assert size == 12
    code = at(b, loads, addr, size)
    assert code == struct.pack('<3I', 0x3c030007, 0x34630000 | index, 12)
    assert code == at(vb, vs, vsyms[vname][0], 12), name

for name, vname, index in [('app_ui_init', 'applib_init', 0x36),
                          ('app_ui_quit', 'applib_quit', 0x37)]:
    addr, size = symbols[name]
    assert size == 12
    code = at(b, loads, addr, size)
    assert code == struct.pack('<3I', 0x3c030012, 0x34630000 | index, 12)
    assert code == at(vb, vs, vsyms[vname][0], 12), name

# All experimental UI traps must also match a named vendor stub.
if 'ui_create_window' in symbols:
    ui_map = {
        'ui_create_window': 'gui_wm_create_window', 'ui_set_focus': 'gui_wm_set_focus',
        'ui_delete_window': 'gui_wm_delete_window', 'ui_dc_get': 'gui_dc_get',
        'ui_default_callback': 'gui_wm_default_callback',
        'ui_create_font': 'gui_create_font', 'ui_destroy_font': 'gui_destroy_font',
        'ui_default_fontface': 'gui_dc_set_default_fontface',
        'ui_text_mode': 'gui_dc_set_text_mode', 'ui_color': 'gui_dc_set_color',
        'ui_font_size': 'gui_dc_set_font_size', 'ui_text': 'gui_dc_display_string_in_rect',
        'ui_font_file': 'sys_get_default_font_file', 'ui_get_msg': 'get_msg',
        'ui_dispatch_msg': 'dispatch_msg', 'ui_register_dispatcher': 'register_sys_dispatcher',
        'ui_unregister_dispatcher': 'unregister_sys_dispatcher', 'ui_exit_loop': 'exit_msg_loop',
    }
    vendors = [('calculat.app', ui_map),
               ('calibrat.app', {'ui_screen_update': 'gui_screen_update'}),
               ('anim_off.app', {'ui_draw_bitmap': 'gui_dc_draw_bitmap_ext'}),
               ('ebook.app', {'ui_clear_rect': 'gui_dc_clear_rect',
                              'ui_background_color': 'gui_dc_set_background_color'}),
               ('manager.app', {'ui_set_timer': 'set_timer', 'ui_kill_timer': 'kill_timer'})]
    for filename, names in vendors:
        raw, _, segs, syms = read_elf(root / 'binaries' / filename)
        for name, vendor_name in names.items():
            addr, size = symbols[name]
            assert size == 12, name
            assert at(b, loads, addr, size) == at(raw, segs, syms[vendor_name][0], size), name
    print('Checked all 24 UI stubs against named vendor app stubs.')

# Fixed-address apps cannot be treated as freely relocatable shared objects.
# Reject overlaps with any load segment in the supplied firmware ELF set.
for p in (root / 'binaries').iterdir():
    raw = p.read_bytes()
    if not raw.startswith(b'\x7fELF'):
        continue
    phoff = struct.unpack_from('<I', raw, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', raw, 42)
    for i in range(phnum):
        typ, off, va, pa, filesz, memsz, flags, align = struct.unpack_from(
            '<8I', raw, phoff+i*phsize)
        if typ != 1 or not memsz:
            continue
        for s in loads:
            assert s[2]+s[5] <= va or va+memsz <= s[2], f'Overlaps {p.name}'
print('Checked app entry, two LOAD segments, BSS, defined symbols,')
print('15 group-7 and two group-18 stubs against calculat.app,')
print('and supplied ELF address ranges.')
