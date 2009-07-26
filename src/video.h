#include "mednafen.h"

static inline void DECOMP_COLOR(int c, int &r, int &g, int &b)
{
 r = (c >> FSettings.rshift) & 0xFF;
 g = (c >> FSettings.gshift) & 0xFF;
 b = (c >> FSettings.bshift) & 0xFF;
}

#define MK_COLOR(r,g,b) ( ((r)<<FSettings.rshift) | ((g) << FSettings.gshift) | ((b) << FSettings.bshift))
#define MK_COLORA(r,g,b, a) ( ((r)<<FSettings.rshift) | ((g) << FSettings.gshift) | ((b) << FSettings.bshift) | ((a) << FSettings.ashift))