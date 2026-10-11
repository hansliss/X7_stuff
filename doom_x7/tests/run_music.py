#!/usr/bin/env python3
"""Bounded host music verification with ASan and UBSan; uses the original WAD."""
from pathlib import Path
import subprocess, tempfile, os, sys
root=Path(__file__).resolve().parents[2]
port=root/'doom_x7'
out=Path(tempfile.mkdtemp(prefix='x7-music-test-'))
engine=out/'engine'
subprocess.run(['python3',str(port/'prepare.py'),str(root/'DOOM/linuxdoom-1.10'),str(engine)],check=True)
# Engine compatibility headers keep libc separate from the freestanding source.
headers=out/'headers';headers.mkdir()
for name in ('stdio.h','stdlib.h','string.h'):
    (headers/name).write_text('#include "x7_runtime.h"\n')
benchmark='--benchmark' in sys.argv
flags=['-std=gnu11','-fms-extensions','-O2','-g','-fwrapv','-fno-strict-aliasing','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(port/'include')]
if benchmark: flags=[f for f in flags if not f.startswith('-fsanitize=') and not f.startswith('-fno-sanitize-recover=')]
objects=[]
for i,source in enumerate((port/'music.c',port/'vendor/rp2040-opl/emu8950.c',port/'tests/music.c')):
    obj=out/f'{i}.o'
    include=[] if i==2 else ['-I'+str(headers),'-I'+str(engine)]
    subprocess.run(['cc',*flags,*include,'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
subprocess.run(['cc',*flags,*objects,'-o',str(out/'music')],check=True)
# The managed command runner uses ptrace, which LeakSanitizer cannot inspect.
# Address/undefined-behavior checks remain enabled; the player allocates no heap.
env=dict(os.environ)
env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
subprocess.run([str(out/'music'),str(root/'DOOM/DOOM.WAD')]+(['benchmark'] if benchmark else ['synthetic'] if '--synthetic' in sys.argv else []),check=True,env=env)
print('Fixture:',out)
