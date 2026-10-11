#ifndef OMEGA_PATHS_H
#define OMEGA_PATHS_H

/* Eight original ROM files and battery RAM share this directory.
 * Edit it and rebuild; use an absolute path without a trailing slash. */
#ifndef OMEGA_RESOURCE_DIR
#define OMEGA_RESOURCE_DIR "/mnt/diska/res/omega"
#endif
#define OMEGA_ROM_DIR OMEGA_RESOURCE_DIR "/"
#define OMEGA_NVRAM_PATH OMEGA_RESOURCE_DIR "/omega-race.nvram"
#define OMEGA_NVRAM_TEMP OMEGA_NVRAM_PATH ".tmp"

#endif
