#!/usr/bin/env python3
"""ROM boot/gameplay, guarded renderer, sanitizers and X7 platform mocks."""
from pathlib import Path
import hashlib, re, shutil, struct, subprocess, tempfile, zlib
root=Path(__file__).resolve().parents[2]
out=Path(tempfile.mkdtemp(prefix='omega-x7-host-'))
port=out/'omega_x7'
shutil.copytree(root/'omega_x7',port)
expected={'omega.m7':0x0424d46e,'omega.l7':0xedcd7a7d,'omega.k7':0x6d10f197,'omega.j7':0x8e8d4b54,'omega.e1':0x1d0fdf3a,'omega.f1':0xd44c0814,'sound.k5':0x7d426017,'dvgprom.bin':0xd481e958}
for name,crc in expected.items(): assert zlib.crc32((root/'omega_race'/name).read_bytes())==crc
flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(port)]
subprocess.run(['cc',*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer',str(port/'vendor/fastz80/z80.c'),str(port/'machine.c'),str(port/'render.c'),str(port/'tests/unit.c'),'-o',str(out/'unit')],check=True)
import os
subprocess.run([str(out/'unit')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
subprocess.run(['cc',*flags,str(port/'vendor/fastz80/z80.c'),str(port/'machine.c'),str(port/'render.c'),str(port/'tests/host.c'),'-o',str(out/'core')],check=True)
subprocess.run([str(out/'core'),str(root/'omega_race'),str(out)],check=True)
# ASan/UBSan execute the actual supplied ROMs as well as the bounded decoder.
subprocess.run(['cc',*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer',str(port/'vendor/fastz80/z80.c'),str(port/'machine.c'),str(port/'render.c'),str(port/'tests/host.c'),'-o',str(out/'sanitized')],check=True)
import os
subprocess.run([str(out/'sanitized'),str(root/'omega_race'),str(out)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},timeout=90)
# PNG previews without external image packages.
def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b))
for p in out.glob('frame-*.ppm'):
 data=p.read_bytes().split(b'\n',3)[3]
 png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',480,272,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+data[y*1440:(y+1)*1440] for y in range(272))))+chunk(b'IEND',b'')
 p.with_suffix('.png').write_bytes(png)
# Reuse the firmware ABI and resource assertions from the validated DOOM fixture.
sysprobe=out/'sysprobe';sysprobe.mkdir()
for name in ('ui_abi.h','app_abi.h','fb_video.h','fb_video.c'):shutil.copy(root/'sysprobe'/name,sysprobe/name)
(out/'syscalls.h').write_text((root/'syscalls.h').read_text().replace('typedef x7_u32 x7_word_t;', 'typedef unsigned long x7_word_t;'))
subprocess.run(['cc',*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer',str(port/'nvram.c'),str(port/'tests/nvram.c'),'-o',str(out/'nvram')],check=True)
nvram_dir=out/'nvram-files';nvram_dir.mkdir()
subprocess.run([str(out/'nvram'),str(nvram_dir)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
fixture=(root/'doom_x7/tests/host_firmware.c').read_text()
fixture=fixture.replace('#include "../include/x7_paths.h"','#include "../paths.h"')
fixture=fixture.replace('if(!strcmp(name,X7_DOOM_WAD_PATH))','if(!strncmp(name,OMEGA_ROM_DIR,sizeof(OMEGA_ROM_DIR)-1)&&strcmp(name,OMEGA_NVRAM_PATH)&&strcmp(name,OMEGA_NVRAM_TEMP))')
fixture=fixture.replace('X7_DOOM_RESOURCE_DIR','OMEGA_RESOURCE_DIR')
fixture=fixture.replace('snprintf(path,sizeof(path),"%s",wad_path);','snprintf(path,sizeof(path),"%s/%s",wad_path,strrchr(name,\'/\')+1);')
fixture=fixture.replace('x<58||x>=421','x<104||x>=376')
# The GUI syscall table is unavailable before gui.so is loaded on hardware.
fixture=fixture.replace('static int scenario,exit_loop,', 'static int gui_loaded;\nstatic int scenario,exit_loop,')
fixture=fixture.replace('if(scenario==8&&!strcmp(name,"gui.so"))return 0;return (void *)1;', 'if(scenario==8&&!strcmp(name,"gui.so"))return 0;if(!strcmp(name,"gui.so"))gui_loaded=1;return (void *)1;')
fixture=fixture.replace('void ui_exit_loop(void){exit_loop=1;}', 'void ui_exit_loop(void){REQUIRE(gui_loaded);exit_loop=1;}')
# Require EOF-independent loading and exercise stat/read/checksum failures.
fixture=fixture.replace('return read(fd-1000,p,n>4096?4096:n);', 'if(scenario==18){struct stat st;REQUIRE(!fstat(fd-1000,&st));if(lseek(fd-1000,0,SEEK_CUR)>=st.st_size)return -5;}if(scenario==20&&lseek(fd-1000,0,SEEK_CUR)>=1024)return -5;ssize_t bytes=read(fd-1000,p,n>4096?4096:n);if(scenario==21&&bytes>0)((unsigned char *)p)[0]^=1;return bytes;')
fixture=fixture.replace('out->st_size=s.st_size;return 0;', 'out->st_size=s.st_size+(scenario==19?1:0);return 0;')

a=fixture.index('        if(submits==300)');b=fixture.index('\n        break;',a);fixture=fixture[:a]+fixture[b:]
a=fixture.index('int ui_get_msg(');b=fixture.index('\nint main(',a)
fixture=fixture[:a]+'''void ui_dispatch_msg(struct ui_app_message *msg){(void)msg;}
int ui_get_msg(struct ui_app_message *msg){
    unsigned calls=0;(void)msg;
    REQUIRE(++get_msg_calls==1);
    while(!exit_loop){
        unsigned i;
        for(i=0;i<8&&!exit_loop;i++)if(timers[i].active){
            if(timers[i].ms==1000){REQUIRE(!cal_delivered);cal_delivered=1;ticks+=100;}else{
                unsigned mask=0;
                ticks++;calls++;
                if(calls>=400&&calls<410)mask|=4;
                if(calls>=500&&calls<510)mask|=8;
                if(calls>=600&&calls<1800)mask|=0x490;
                if(calls>=3000)mask|=0x200000;
                if(mask!=raw_key_state){
                    struct ui_key key={raw_key_state&~mask,0x04000000};struct ui_message event={.msgid=13,.data.key=&key};
                    if(key.val)deliver_key(&event);
                    key.val=mask&~raw_key_state;key.type=0x20000000;if(key.val)deliver_key(&event);
                }
            }
            timers[i].cb(timers[i].arg);
        }
        REQUIRE(calls<4000);
    }
    return 0;
}
''' + fixture[b:]
# Model actual queued playback without the earlier Doom fixture's minimum floor.
fixture=fixture.replace('unsigned consumed=(ticks-audio_epoch)*441;', 'unsigned consumed=audio_playing?(ticks-audio_epoch)*441:0;')
fixture=fixture.replace('if(scenario!=13&&audio_submits>=3&&audio_queue<1536)audio_queue=1536;', '')
fixture=fixture.replace('case 0x4d414f02:if(scenario==12)return -1;audio_playing=1;break;', 'case 0x4d414f02:if(scenario==12)return -1;audio_epoch=ticks;audio_playing=1;break;')
(port/'tests/firmware.c').write_text(fixture)
abi=['-std=gnu11','-O2','-g','-D_Static_assert(x,y)=','-DX7_PLAYABLE=1','-DX7_REALTIME_AUDIO=1']
platform_sources=[str(port/'vendor/fastz80/z80.c'),str(port/'machine.c'),str(port/'render.c'),str(port/'platform.c'),str(port/'nvram.c'),str(sysprobe/'fb_video.c'),str(port/'tests/firmware.c')]
subprocess.run(['cc',*abi,'-DX7_OMEGA_DEBUG_LOG=1',*platform_sources,'-o',str(out/'platform')],check=True)
for scenario in (0,1,2,3,5,6,8,9,10,11,12,18,19,20,21):
 run=out/f'platform-{scenario}';run.mkdir()
 result=subprocess.run([str(out/'platform'),str(run),str(root/'omega_race'),str(scenario)],timeout=60)
 assert result.returncode==(0 if scenario in (0,18) else 1),(scenario,result.returncode)
 if scenario==0:
  values={tag:int(v,16) for tag,v in re.findall(r'^(.*?) 0x([0-9a-f]+)$',(run/'omega-x7.txt').read_text(),re.M)}
  assert values['vector lists']>100 and values['vector errors']==0 and values['AY writes']>0
  assert values['audio ring overflow']==0 and values['empty audio queries']==0,values
  assert values['frames']>500 and values['main CPU cycles']>80000000,values
  saved=(run/'omega-race.nvram').read_bytes()
  assert len(saved)==272 and saved[:8]==b'X7ORNV1\0'
  assert zlib.crc32(saved[16:])==struct.unpack_from('<I',saved,12)[0]
  # A second process restores exactly the board RAM produced by the ROM.
  subprocess.run([str(out/'platform'),str(run),str(root/'omega_race'),'0'],check=True,timeout=60)
  restored={tag:int(v,16) for tag,v in re.findall(r'^(.*?) 0x([0-9a-f]+)$',(run/'omega-x7.txt').read_text(),re.M)}
  assert restored['NVRAM load result']==0
subprocess.run(['cc',*abi,*platform_sources,'-o',str(out/'release')],check=True)
for scenario in (0,1,5):
 run=out/f'release-{scenario}';run.mkdir()
 result=subprocess.run([str(out/'release'),str(run),str(root/'omega_race'),str(scenario)],timeout=60)
 assert result.returncode==(0 if scenario==0 else 1)
 assert not (run/'omega-x7.txt').exists()
 assert (run/'omega-x7-error.txt').exists()==bool(scenario)
 if not scenario:
  assert (run/'omega-race.nvram').exists()
  stale=run/'omega-x7.txt';stale.write_text('retained old diagnostic log\n')
  subprocess.run([str(out/'release'),str(run),str(root/'omega_race'),'0'],check=True,timeout=60)
  assert stale.read_text()=='retained old diagnostic log\n'
  # Corruption is reported without preventing play, and no normal log is opened.
  saved=bytearray((run/'omega-race.nvram').read_bytes());saved[12]^=1
  (run/'omega-race.nvram').write_bytes(saved)
  subprocess.run([str(out/'release'),str(run),str(root/'omega_race'),'0'],check=True,timeout=60)
  assert 'NVRAM invalid or unreadable' in (run/'omega-x7-error.txt').read_text()
  assert stale.read_text()=='retained old diagnostic log\n'
print('ROM checks, sanitizer run, gameplay, audio queue and platform cleanup passed.')
print('Host artifacts:',out)
