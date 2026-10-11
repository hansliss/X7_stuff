#ifndef X7_FRAGMENT_H
#define X7_FRAGMENT_H
#include "dodeca_render.h"
/* Flat geometric normals, but lighting evaluated separately at each fragment. */
struct fragment_face { struct vec normal, light; int plane; unsigned palette; };
int fragment_init(struct vec camera,struct vec right,struct vec up,
                  struct vec forward,int focal_length);
void fragment_prepare(struct fragment_face *,unsigned face,struct vec normal,
                      struct vec surface_point,struct vec light);
unsigned fragment_intensity(struct vec normal,struct vec position,struct vec light);
unsigned short fragment_pixel(const struct fragment_face *,int x,int y);
#endif
