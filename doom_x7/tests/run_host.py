#!/usr/bin/env python3
"""Run the actual engine against bounded firmware mocks (Linux x86-64).

Temporary host-width pointers and low mmap allocations accommodate the old
engine's 32-bit assumptions. This does not execute MIPS or model LCD timing.
"""
import pathlib
import re
import shutil
import subprocess
import tempfile
import sys
if any(arg not in ("--play",) for arg in sys.argv[1:]):
    raise SystemExit("Usage: run_host.py [--play] (the playable app is the sole supported build)")
play = True
audio = True

root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(tempfile.mkdtemp(prefix='doom-x7-host-'))
port = out / 'doom_x7'
shutil.copytree(root / 'doom_x7', port)
sysprobe = out / 'sysprobe'
sysprobe.mkdir()
for name in ('ui_abi.h', 'app_abi.h', 'fb_video.h', 'fb_video.c'):
    shutil.copy(root / 'sysprobe' / name, sysprobe / name)
text = (root / 'syscalls.h').read_text().replace('typedef x7_u32 x7_word_t;', 'typedef unsigned long x7_word_t;')
(out / 'syscalls.h').write_text(text)
engine = out / 'engine'
subprocess.run(['python3', str(port / 'prepare.py'), str(root / 'DOOM/linuxdoom-1.10'), str(engine)], check=True)
defaults=engine/'m_misc.c'
text=defaults.read_text()
text=re.sub(r'int\s+defaultvalue;', 'long defaultvalue;', text)
text=re.sub(r'\(int\)\s*(HUSTR_\w+|"[^"]*")', r'(long) \1', text)
defaults.write_text(text)
headers = out / 'headers'
(headers / 'sys').mkdir(parents=True)
for name in ('stdio.h', 'stdlib.h', 'string.h', 'ctype.h', 'values.h', 'fcntl.h', 'unistd.h', 'malloc.h', 'sys/stat.h', 'sys/types.h'):
    (headers / name).write_text('#include "x7_runtime.h"\n')
(headers / 'math.h').write_text('')
(headers / 'alloca.h').write_text('#define alloca(n) __builtin_alloca(n)\n')

# Avoid interposing compatibility libc symbols on the host's libc.
runtime = (port / 'include/x7_runtime.h').read_text()
renames = sorted(set(re.findall(r'\b([a-z][a-z0-9_]*)\s*\(', runtime)) - {'__attribute__'})
renames = [n for n in renames if not n.startswith('x7_')]
renames += ['stdin', 'stdout', 'stderr']
flags = ['-std=gnu11', '-fms-extensions', '-O2', '-g', '-fwrapv', '-fno-strict-aliasing', '-fno-builtin',
         '-fno-pie', '-D_Static_assert(x,y)=', '-DNORMALUNIX', '-DLINUX',
         '-I'+str(headers), '-I'+str(port/'include'), '-I'+str(engine),
         '-include', str(port/'include/x7_runtime.h')]
flags += ['-D'+n+'=doom_'+n for n in renames]
excluded = {'i_main.c','i_system.c','i_video.c','i_sound.c','i_net.c','m_fixed.c','s_sound.c'}
if audio: excluded.remove('s_sound.c')
sources = [p for p in sorted(engine.glob('*.c')) if p.name not in excluded]
sources += [port/n for n in ('platform.c', 'runtime.c', 'fixed.c', 'audio.c')]
if play: sources += [port/'input.c',port/'display.c',port/'settings.c',port/'music.c',port/'vendor/rp2040-opl/emu8950.c']
sources += [sysprobe/'fb_video.c']
if audio: flags += ['-DX7_REALTIME_AUDIO=1']
if play: flags += ['-DX7_PLAYABLE=1','-DX7_DOOM_DEBUG_LOG=1']
objects = []
with (out/'compile.log').open('w') as log:
    for i, source in enumerate(sources):
        obj = out/f'{i}.o'
        subprocess.run(['cc', *flags, '-c', str(source), '-o', str(obj)], stdout=log, stderr=log, check=True)
        objects.append(str(obj))
    mock = out/'mock.o'
    subprocess.run(['cc','-std=gnu11','-fms-extensions','-O2','-g','-fno-pie','-D_Static_assert(x,y)=',
                    *(['-DX7_REALTIME_AUDIO=1'] if audio else []), *(['-DX7_PLAYABLE=1'] if play else []), '-c',str(port/'tests/host_firmware.c'),'-o',str(mock)], stdout=log, stderr=log, check=True)
    subprocess.run(['cc','-no-pie',*objects,str(mock),'-o',str(out/'integration')],stdout=log,stderr=log,check=True)

def values(path):
    return {tag:int(value,16) for tag,value in re.findall(r'^(.*?) 0x([0-9a-f]+)$',path.read_text(),re.M)}

for mode,demo in (() if play else ((1,1),(2,1),(3,1),(1,2),(1,3))):
    run = out/f'mode{mode}-demo{demo}'
    run.mkdir()
    (run/'doom-x7-mode.txt').write_text(f'{mode}{demo}')
    subprocess.run([str(out/'integration'),str(run),str(root/'DOOM/DOOM.WAD'),'0'],check=True,timeout=60)
    data=values(run/('doom-x7-play.txt' if play else ('doom-x7-audio.txt' if audio else 'doom-x7-benchmark.txt')))
    expected={1:1710,2:2347,3:3863}[demo]
    assert data['completed simulation tics']==expected,data
    assert data['exit reason (1=demo complete 2=Start 3=system 4=error)']==1,data
    assert data['submitted frames']==(expected if mode==1 else 0),data
    assert data['rendered frames']==(expected if mode<3 else 0),data
    if audio:
        assert abs(data['measured elapsed OS ticks']-expected*100/35)<10,data
        assert data['audio sound starts']>0,data
        assert data['audio blocks submitted']>expected*44100//35//1024*9//10,data
        log=(run/'doom-x7-audio.txt').read_text()
        assert [int(v,16) for v in re.findall(r'^audio gain divisor 0x([0-9a-f]+)$',log,re.M)]==[2,3,4]
        assert all(int(v,16)>0 for v in re.findall(r'^audio gain blocks 0x([0-9a-f]+)$',log,re.M))
        assert all(int(v,16)==0 for v in re.findall(r'^audio clipped samples 0x([0-9a-f]+)$',log,re.M))
    print(f'Mode {mode}, DEMO{demo}: {expected} tics, clean completion')

for scenario in range(1,13 if audio else 9):
    run = out/f'failure{scenario}'
    run.mkdir()
    result=subprocess.run([str(out/'integration'),str(run),str(root/'DOOM/DOOM.WAD'),str(scenario)],timeout=60)
    assert result.returncode==(0 if scenario==4 else 1),result
    data=values(run/('doom-x7-play.txt' if play else ('doom-x7-audio.txt' if audio else 'doom-x7-benchmark.txt')))
    assert data['exit reason (1=demo complete 2=Start 3=system 4=error)']==(2 if scenario==4 else 4),data
    assert 'DONE' in data
print('Host artifacts:',out)

if audio and not play:
    run = out/'audio-long-callbacks'
    run.mkdir()
    (run/'doom-x7-mode.txt').write_text('11')
    subprocess.run([str(out/'integration'),str(run),str(root/'DOOM/DOOM.WAD'),'13'],check=True,timeout=60)
    data=values(run/'doom-x7-audio.txt')
    assert data['completed simulation tics']==1710,data
    assert data['audio maximum pump gap OS ticks']>=15,data
    print('Audio: repeated 150 ms callback gaps, no modeled queue starvation')

if play:
    run=out/'playable'
    run.mkdir()
    subprocess.run([str(out/'integration'),str(run),str(root/'DOOM/DOOM.WAD'),'14'],check=True,timeout=60)
    data=values(run/'doom-x7-play.txt')
    assert data['exit reason (1=demo complete 2=Start 3=system 4=error)']==2,data
    assert data['completed simulation tics']>100,data
    assert data['input key presses']>=6 and data['input key releases']>=4,data
    assert data['startup menu active']==0,data
    assert data['startup waits for key']==1,data
    assert data['calibration interval requested milliseconds']==1000,data
    assert data['startup loading yield ticks']<data['startup loading read checkpoints']//8,data
    assert data['startup elapsed OS ticks']<200,data
    assert data['settings saved']==1,data
    assert data['startup game state']==3,data
    assert data['display output width']==363 and data['display output height']==272,data
    assert data['raw combined states']>0,data
    assert data['set raw key driver mode result']==0,data
    assert data['restore key driver mode result']==0,data
    assert data['music note starts']>0 and data['music events']>0,data
    assert data['music malformed scores']==0,data
    assert data['music peak before master gain']>0,data
    assert data['player ready weapon']==0,data # Y selects fist from pistol
    assert data['player bullet ammo']<50,data
    assert data['player y']!=0xfffffffff1e00000,data
    print('Playable: movement, firing, held/released controls, menu and launcher exit checked')

    subprocess.run(['cc','-std=gnu11','-fms-extensions','-O2','-fno-builtin','-D_Static_assert(x,y)=',
                    '-I'+str(headers),'-I'+str(port/'include'),'-I'+str(engine),
                    str(port/'input.c'),str(port/'tests/input.c'),'-o',str(out/'input-test')],check=True)
    subprocess.run([str(out/'input-test')],check=True)
    print('Input: simultaneous buttons, all UP classes, held repeats, menu context and weapon cycle checked')

if play:
    subprocess.run(['cc','-std=c11','-O2','-I'+str(port/'include'),
                    str(port/'display.c'),str(port/'tests/display.c'),
                    '-o',str(out/'display-test')],check=True)
    subprocess.run([str(out/'display-test')],check=True)
    print('Display: all scaled pixels, side margins and buffer guards checked')

if play:
    defaults={'x7_config_version':1,'mouse_sensitivity':5,'sfx_volume':8,
              'music_volume':8,'show_messages':1,'screenblocks':10,
              'detaillevel':0,'usegamma':0}
    expected=dict(defaults,mouse_sensitivity=7,sfx_volume=3,music_volume=11,
                  show_messages=0,screenblocks=9,detaillevel=1,usegamma=2)
    persistent=out/'settings-roundtrip'
    persistent.mkdir()
    config=persistent/'doom.cfg'
    seed=''.join(f'{k} {v}\n' for k,v in expected.items())
    seed+='unknown 999\nsfx_volume 999999999999999999999\nusegamma -1\n'
    config.write_text(seed)
    for repeat in range(2):
        subprocess.run([str(out/'integration'),str(persistent),str(root/'DOOM/DOOM.WAD'),'14'],check=True,timeout=60)
        saved={k:int(v) for k,v in re.findall(r'^(\w+) (\d+)$',config.read_text(),re.M)}
        assert saved==expected,saved
        log=(persistent/'doom-x7-play.txt').read_text()
        assert 'S_Init: default sfx volume 3' in log
        assert values(persistent/'doom-x7-play.txt')['settings loaded entries']==7
    for name,content in [('wrong-version','x7_config_version 2\nsfx_volume 3\n'),
                         ('oversized','x7_config_version 1\nsfx_volume 3\n'+'#'*3000)]:
        invalid=out/('settings-'+name)
        invalid.mkdir()
        (invalid/'doom.cfg').write_text(content)
        subprocess.run([str(out/'integration'),str(invalid),str(root/'DOOM/DOOM.WAD'),'14'],check=True,timeout=60)
        saved={k:int(v) for k,v in re.findall(r'^(\w+) (\d+)$',(invalid/'doom.cfg').read_text(),re.M)}
        assert saved==defaults,saved
    config.write_text(seed)
    subprocess.run([str(out/'integration'),str(persistent),str(root/'DOOM/DOOM.WAD'),'17'],check=True,timeout=60)
    assert config.read_text()==seed
    assert values(persistent/'doom-x7-play.txt')['settings saved']==0
    print('Settings: restart round-trip, invalid/range/oversized input, and failed replacement preserving old config checked')

# Run the production platform with logging disabled using the same engine/mocks.
if play:
    release_flags=[f for f in flags if f != '-DX7_DOOM_DEBUG_LOG=1']+['-DX7_DOOM_DEBUG_LOG=0']
    platform_index=sources.index(port/'platform.c')
    release_object=out/'platform-release.o'
    with (out/'compile.log').open('a') as log:
        subprocess.run(['cc',*release_flags,'-c',str(port/'platform.c'),'-o',str(release_object)],stdout=log,stderr=log,check=True)
        release_objects=list(objects)
        release_objects[platform_index]=str(release_object)
        subprocess.run(['cc','-no-pie',*release_objects,str(mock),'-o',str(out/'release')],stdout=log,stderr=log,check=True)
    run=out/'release-playable'
    run.mkdir()
    subprocess.run([str(out/'release'),str(run),str(root/'DOOM/DOOM.WAD'),'14'],check=True,timeout=60)
    assert not (run/'doom-x7-play.txt').exists()
    assert (run/'doom.cfg').exists()
    print('Release: gameplay, cleanup and settings checked with routine logging disabled')
