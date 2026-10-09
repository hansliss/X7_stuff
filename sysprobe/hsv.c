#include "hsv.h"
unsigned hsv_rgb(unsigned hue, unsigned saturation, unsigned value)
{
    unsigned sector, fraction, p, q, t, r, g, b;
    hue %= 1536;
    sector = hue >> 8;
    fraction = hue & 255;
    p = (value * (255 - saturation) + 127) / 255;
    q = (value * (255 - (saturation * fraction + 127) / 255) + 127) / 255;
    t = (value * (255 - (saturation * (255 - fraction) + 127) / 255) + 127) / 255;
    switch (sector) {
    case 0: r=value; g=t; b=p; break;
    case 1: r=q; g=value; b=p; break;
    case 2: r=p; g=value; b=t; break;
    case 3: r=p; g=q; b=value; break;
    case 4: r=t; g=p; b=value; break;
    default: r=value; g=p; b=q; break;
    }
    return (r << 16) | (g << 8) | b;
}

unsigned short hsv_gui_pixel(unsigned color)
{
    return (unsigned short)(((color >> 19) & 31) |
                            (((color >> 10) & 63) << 5) |
                            (((color >> 3) & 31) << 11));
}
