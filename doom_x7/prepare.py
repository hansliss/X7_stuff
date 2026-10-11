#!/usr/bin/env python3
"""Generate explicit X7 adaptations without changing the cloned source."""
import pathlib
import re
import sys

source, dest = map(pathlib.Path, sys.argv[1:3])
dest.mkdir(parents=True, exist_ok=True)


def function(text, name, body):
    match = re.search(r'\b' + name + r'\s*\([^;{}]*\)\s*\{', text)
    if not match:
        raise RuntimeError(f"Cannot locate {name}")
    start = match.end() - 1
    depth, end = 1, start + 1
    # These selected functions have balanced braces in comments/strings too.
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + '{\n' + body + '\n}' + text[end:]


for path in list(source.glob('*.c')) + list(source.glob('*.h')):
    text = path.read_text()
    # Remove undefined double writes to event ring indices.
    text = text.replace('(++eventhead)', '(eventhead+1)').replace('(++eventtail)', '(eventtail+1)')
    # R_InitSpriteDefs expects a NULL terminator; the original table omits it.
    text = text.replace('sprnames[NUMSPRITES]', 'sprnames[NUMSPRITES+1]')
    if path.name in ('d_main.c', 'm_menu.c', 'g_game.c'):
        text = '#include "x7_paths.h"\n' + text
        text = text.replace('SAVEGAMENAME"', 'X7_DOOM_SAVE_PREFIX SAVEGAMENAME"')
    if path.name.startswith('p_'):
        text = re.sub(r'\bopen\b', 'door_open', text)
        text = re.sub(r'\bclose\b', 'door_close', text)
    if path.name == 'd_main.c':
        text = function(text, 'IdentifyVersion', '''
    gamemode = registered;
    strcpy(basedefault, X7_DOOM_CONFIG_PATH);
    D_AddFile(X7_DOOM_WAD_PATH);''')
        text = function(text, 'FindResponseFile', '/* Fixed benchmark arguments. */')
        text = function(text, 'D_DoomLoop', 'x7_run_loop();')
        text = re.sub(r'(void\s+D_PageTicker\s*\(void\)\s*\{)', r'\1\n#ifdef X7_PLAYABLE\n    return; /* Keep the title page behind the startup menu. */\n#endif', text)
        text = text.replace('D_DoomLoop ();  // never returns',
                            'D_DoomLoop (); return; /* X7 loop returns for cleanup. */')
        text = text.replace('NetUpdate ();', '#ifndef X7_PLAYABLE\n    NetUpdate ();\n#endif\n')
        # Timedemo must not busy-wait inside GUI callbacks for a wipe's clock.
        text = text.replace('if (gamestate != wipegamestate)',
                            'if (0 /* benchmark: disable transitional wipes */)')
    elif path.name == 'd_net.c':
        text = re.sub(r'(void\s+NetUpdate\s*\(void\)\s*\{)', r'\1\n#ifdef X7_PLAYABLE\n    return; /* The platform owns single-player command production. */\n#endif', text)
    elif path.name == 'm_menu.c':
        text = text.replace('    inhelpscreens = false;', '#ifdef X7_PLAYABLE\n    if (gamestate == GS_DEMOSCREEN && (menuactive || messageToPrint))\n        memset(screens[0], 0, SCREENWIDTH*SCREENHEIGHT);\n#endif\n    inhelpscreens = false;')
        text = text.replace('savegamestrings[choice][0] = 0;', 'snprintf(savegamestrings[choice], sizeof(savegamestrings[choice]), "X7 SLOT %d", choice+1);')
    elif path.name == 'm_misc.c':
        text = function(text, 'M_SaveDefaults', '#ifdef X7_PLAYABLE\n    extern void x7_settings_save(void);\n    x7_settings_save();\n#endif')
        text = function(text, 'M_LoadDefaults', '''
    int i;
    numdefaults = sizeof(defaults)/sizeof(defaults[0]);
    for(i=0;i<numdefaults;i++) *defaults[i].location=defaults[i].defaultvalue;
    defaultfile=basedefault;
    screenblocks=10; detailLevel=0;
#ifdef X7_PLAYABLE
    {extern void x7_settings_load(void);x7_settings_load();}
#endif''')
    elif path.name == 's_sound.c':
        text = '#ifdef X7_PLAYABLE\n#include "x7_music.h"\n#endif\n' + text
        text = text.replace('music->handle = I_RegisterSong(music->data);', '''#ifdef X7_PLAYABLE
    music->handle = x7_music_register(music->data, W_LumpLength(music->lumpnum));
#else
    music->handle = I_RegisterSong(music->data);
#endif''')
    elif path.name == 'g_game.c':
        old = 'if ( *demo_p++ != VERSION)'
        assert old in text
        text = text.replace(old, 'if ( *demo_p != 109 && *demo_p != VERSION)')
        text = text.replace('fprintf( stderr, "Demo is from a different game version!\\n");',
                            'I_Error("Unsupported demo version: %i", *demo_p);')
        # Consume version only after validation (no format changes for v109).
        text = text.replace('skill = *demo_p++;', 'demo_p++;\n    skill = *demo_p++;')
        text = text.replace('demoplayback = true;', 'demoplayback = true;\n    x7_benchmark_begin();')
        text = re.sub(r'I_Error\s*\("timed %i gametics in %i realtics",gametic\s*, endtime-starttime\);',
                      'x7_benchmark_done();\n        return true;', text)
    elif path.name == 'w_wad.c':
        # Both directory readers share a bounded temporary buffer, never alloca.
        text = 'static unsigned int x7_wad_directory[16384];\n' + text
        text = text.replace('fileinfo = alloca (length);', '''
        if (length < 0 || (unsigned)length > sizeof(x7_wad_directory))
            I_Error("WAD directory exceeds prototype limit");
        fileinfo = (filelump_t *)x7_wad_directory;''')
    elif path.name == 'r_data.c':
        text = re.sub(r'void\s+\*\*columndirectory;',
                      'unsigned int columndirectory;', text)
        for name in ('textures', 'texturecolumnlump', 'texturecolumnofs', 'texturecomposite'):
            text = text.replace(name + ' = Z_Malloc (numtextures*4,',
                                name + ' = Z_Malloc (numtextures*sizeof(*' + name + '),')
    elif path.name == 'p_setup.c':
        text = text.replace('linebuffer = Z_Malloc (total*4,',
                            'linebuffer = Z_Malloc (total*sizeof(*linebuffer),')
    (dest / path.name).write_text(text)
