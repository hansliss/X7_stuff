#ifndef X7_DODECA_RENDER_H
#define X7_DODECA_RENDER_H
#define DODECA_SCREEN_WIDTH 480
#define DODECA_SCREEN_HEIGHT 272
/* Object fits this centered update rectangle throughout its rotation. */
#ifdef PROBE_FB
#define DODECA_WIDTH 480
#define DODECA_HEIGHT 272
#else
#define DODECA_WIDTH 240
#define DODECA_HEIGHT 224
#endif
#define DODECA_X ((DODECA_SCREEN_WIDTH-DODECA_WIDTH)/2)
#define DODECA_Y ((DODECA_SCREEN_HEIGHT-DODECA_HEIGHT)/2)
#define DODECA_UNIT 1024
struct vec { int x,y,z; };
/* Positions use 1024 units per world unit; focal length is in pixels. */
struct dodeca_scene {
    struct vec camera_position, look_at, up, light_position;
    int focal_length;
};
extern const struct dodeca_scene dodeca_default_scene;
int dodeca_init(const struct dodeca_scene *scene);
unsigned dodeca_render(unsigned short *pixels, unsigned frame);
#endif
