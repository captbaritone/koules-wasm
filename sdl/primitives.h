/* sdl/primitives.h - surface-based 2D primitives (see primitives.c). */
#ifndef _KOULES_SDL_PRIMITIVES_H
#define _KOULES_SDL_PRIMITIVES_H

#include <SDL.h>

void gfxPrimitivesSetFont (const void *fontdata, Uint32 cw, Uint32 ch);

extern unsigned char koules_fontdata[256 * 8];

int pixelColor (SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color);
int lineColor (SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
	       Uint32 color);
int thickLineColor (SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2,
		    Sint16 y2, Uint8 width, Uint32 color);
int rectangleColor (SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2,
		    Sint16 y2, Uint32 color);
int circleColor (SDL_Surface *dst, Sint16 x0, Sint16 y0, Sint16 radius,
		 Uint32 color);
int filledCircleColor (SDL_Surface *dst, Sint16 x0, Sint16 y0, Sint16 radius,
		       Uint32 color);
int stringColor (SDL_Surface *dst, Sint16 x, Sint16 y, const char *s,
		 Uint32 color);

#endif /* _KOULES_SDL_PRIMITIVES_H */
