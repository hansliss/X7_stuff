#ifndef X7_FB_VIDEO_H
#define X7_FB_VIDEO_H
#include "../syscalls.h"
/* Vendor FB.KO/LIBEMU.SO ABI, not Linux fbdev. */
struct fb_video { int fd, started, blending; void *pool; x7_word_t address[5],recent[2]; unsigned avoided,unavailable,query_failures; };
int fb_video_open(struct fb_video *, int (*)(const char *,x7_u64), int (*)(const char *));
unsigned short *fb_video_pixels(struct fb_video *,unsigned);
int fb_video_acquire(struct fb_video *,unsigned,unsigned *);
int fb_video_submit(struct fb_video *,unsigned);
int fb_video_close(struct fb_video *,int (*)(const char *,x7_u64),int (*)(const char *));
#endif
