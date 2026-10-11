#ifndef OMEGA_NVRAM_H
#define OMEGA_NVRAM_H
#include "paths.h"

#define OMEGA_NVRAM_SIZE 256

/* Load: 0 restored, 1 absent, -1 invalid/unreadable. Failure leaves RAM intact.
 * Save: 0 committed, -1 failed; never truncate the previous committed file. */
int omega_nvram_load(unsigned char *ram);
int omega_nvram_save(const unsigned char *ram);

#endif
