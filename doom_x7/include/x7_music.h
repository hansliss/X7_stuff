#ifndef X7_MUSIC_H
#define X7_MUSIC_H
int x7_music_register(void *data, int length);
int x7_music_sample(void);
/* Exposed for the bounded host fixture; no filesystem or firmware dependency. */
int x7_music_set_bank(const unsigned char *data, unsigned length);
unsigned x7_music_errors(void);
unsigned x7_music_loops(void);
#endif
