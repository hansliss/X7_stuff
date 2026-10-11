#ifndef X7_DOOM_PATHS_H
#define X7_DOOM_PATHS_H

/* All game assets and persistent files. Edit this directory and rebuild.
 * Use an absolute path without a trailing slash; launcher CWD is unspecified. */
#ifndef X7_DOOM_RESOURCE_DIR
#define X7_DOOM_RESOURCE_DIR "/mnt/diska/res/doom"
#endif
#define X7_DOOM_WAD_PATH X7_DOOM_RESOURCE_DIR "/doom.wad"
#define X7_DOOM_CONFIG_PATH X7_DOOM_RESOURCE_DIR "/doom.cfg"
#define X7_DOOM_SAVE_PREFIX X7_DOOM_RESOURCE_DIR "/"

#endif
