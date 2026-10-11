#!/usr/bin/env python3
"""Compare v4 and antialiased drawing of identical actual-ROM vector captures."""
from pathlib import Path
import argparse, subprocess, tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--frames',type=int,default=100000,help='replays per captured scene')
args=parser.parse_args()
if args.frames<1:parser.error('--frames must be positive')
root=Path(__file__).resolve().parents[2]
port=root/'omega_x7'
out=Path(tempfile.mkdtemp(prefix='omega-render-benchmark-'))
flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(port)]
subprocess.run(['cc',*flags,str(port/'vendor/fastz80/z80.c'),str(port/'machine.c'),str(port/'render.c'),str(port/'tests/host.c'),'-o',str(out/'capture')],check=True)
subprocess.run([str(out/'capture'),str(root/'omega_race'),str(out),'--benchmark'],check=True)
subprocess.run(['cc',*flags,'-DOMEGA_RENDER_BASELINE',str(port/'render.c'),str(port/'tests/render_baseline.c'),str(port/'tests/render_benchmark.c'),'-o',str(out/'render')],check=True)
for lines in sorted(out.glob('lines-*.txt')):
    subprocess.run([str(out/'render'),str(lines),str(args.frames),str(out/lines.stem)],check=True)
print('Host comparisons only; native rendering cost requires a console test.')
print('Line captures and before/after PPMs:',out)
