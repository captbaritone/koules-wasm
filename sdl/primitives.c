/* sdl/primitives.c - minimal 2D primitives drawn on SDL_Surface.
 *
 * The SDL 1.2 backend used SDL_gfx's *surface-based* primitives
 * (pixelColor, lineColor, ...). SDL2_gfx only offers renderer-based
 * ones, which don't fit this codebase: the game draws into offscreen
 * surfaces, blits between them, and even reads pixels back
 * (SGetPixel). So the handful of primitives the game actually uses
 * are reimplemented here directly on SDL_Surface.
 *
 * Color convention matches SDL_gfx: Uint32 0xRRGGBBAA.
 * All coordinates are clipped to the surface. Surfaces are assumed
 * 32-bit (the backend always creates them so); single-threaded,
 * software surfaces, no locking needed.
 */

#include <SDL.h>

#include "primitives.h"

extern unsigned char koules_fontdata[256 * 8];

static const unsigned char *fontdata = koules_fontdata;

void
gfxPrimitivesSetFont (const void *data, Uint32 cw, Uint32 ch)
{
  (void) cw;
  (void) ch;
  if (data)
    fontdata = data;
}

/* Map 0xRRGGBBAA onto the surface's pixel format. */
static Uint32
map_color (SDL_Surface *dst, Uint32 color, Uint8 *a_out)
{
  Uint8           r = (color >> 24) & 0xff;
  Uint8           g = (color >> 16) & 0xff;
  Uint8           b = (color >> 8) & 0xff;
  Uint8           a = color & 0xff;

  *a_out = a;
  return SDL_MapRGBA (dst->format, r, g, b, a);
}

static void
putpixel (SDL_Surface *dst, int x, int y, Uint32 color)
{
  Uint32         *pixels = (Uint32 *) dst->pixels;
  Uint8           a;
  Uint32          mapped = map_color (dst, color, &a);

  if (x < 0 || y < 0 || x >= dst->w || y >= dst->h)
    return;
  if (a == 0xff)
    {
      pixels[y * dst->w + x] = mapped;
    }
  else if (a != 0)
    {
      /* Alpha blend over destination. */
      Uint8           dr, dg, db, da;
      Uint32          d = pixels[y * dst->w + x];
      Uint8           sr = (color >> 24) & 0xff;
      Uint8           sg = (color >> 16) & 0xff;
      Uint8           sb = (color >> 8) & 0xff;

      SDL_GetRGBA (d, dst->format, &dr, &dg, &db, &da);
      dr = (sr * a + dr * (255 - a)) / 255;
      dg = (sg * a + dg * (255 - a)) / 255;
      db = (sb * a + db * (255 - a)) / 255;
      pixels[y * dst->w + x] = SDL_MapRGBA (dst->format, dr, dg, db, 0xff);
    }
}

int
pixelColor (SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color)
{
  putpixel (dst, x, y, color);
  return 0;
}

static void
hline (SDL_Surface *dst, int x1, int x2, int y, Uint32 color)
{
  int             x, tmp;

  if (y < 0 || y >= dst->h)
    return;
  if (x1 > x2)
    tmp = x1, x1 = x2, x2 = tmp;
  if (x1 < 0)
    x1 = 0;
  if (x2 >= dst->w)
    x2 = dst->w - 1;
  for (x = x1; x <= x2; x++)
    putpixel (dst, x, y, color);
}

int
lineColor (SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
	   Uint32 color)
{
  int             dx = x2 - x1, dy = y2 - y1;
  int             sx = (dx > 0) ? 1 : -1, sy = (dy > 0) ? 1 : -1;
  int             err, e2;

  dx = dx < 0 ? -dx : dx;
  dy = dy < 0 ? -dy : dy;
  err = dx - dy;

  while (1)
    {
      putpixel (dst, x1, y1, color);
      if (x1 == x2 && y1 == y2)
	break;
      e2 = 2 * err;
      if (e2 > -dy)
	{
	  err -= dy;
	  x1 += sx;
	}
      if (e2 < dx)
	{
	  err += dx;
	  y1 += sy;
	}
    }
  return 0;
}

/* Thick line by stamping filled circles along the path. */
int
thickLineColor (SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
		Uint8 width, Uint32 color)
{
  int             dx = x2 - x1, dy = y2 - y1;
  int             steps, i;
  int             r = width / 2;

  steps = dx < 0 ? -dx : dx;
  if ((dy < 0 ? -dy : dy) > steps)
    steps = dy < 0 ? -dy : dy;
  if (steps == 0)
    steps = 1;
  for (i = 0; i <= steps; i++)
    {
      int             x = x1 + dx * i / steps;
      int             y = y1 + dy * i / steps;
      filledCircleColor (dst, x, y, r, color);
    }
  return 0;
}

int
rectangleColor (SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
		Uint32 color)
{
  lineColor (dst, x1, y1, x2, y1, color);
  lineColor (dst, x2, y1, x2, y2, color);
  lineColor (dst, x2, y2, x1, y2, color);
  lineColor (dst, x1, y2, x1, y1, color);
  return 0;
}

int
circleColor (SDL_Surface *dst, Sint16 x0, Sint16 y0, Sint16 radius,
	     Uint32 color)
{
  int             x = radius, y = 0;
  int             err = 0;

  if (radius <= 0)
    {
      putpixel (dst, x0, y0, color);
      return 0;
    }
  while (x >= y)
    {
      putpixel (dst, x0 + x, y0 + y, color);
      putpixel (dst, x0 + y, y0 + x, color);
      putpixel (dst, x0 - y, y0 + x, color);
      putpixel (dst, x0 - x, y0 + y, color);
      putpixel (dst, x0 - x, y0 - y, color);
      putpixel (dst, x0 - y, y0 - x, color);
      putpixel (dst, x0 + y, y0 - x, color);
      putpixel (dst, x0 + x, y0 - y, color);
      y++;
      err += 1 + 2 * y;
      if (2 * (err - x) + 1 > 0)
	{
	  x--;
	  err += 1 - 2 * x;
	}
    }
  return 0;
}

int
filledCircleColor (SDL_Surface *dst, Sint16 x0, Sint16 y0, Sint16 radius,
		   Uint32 color)
{
  int             x = radius, y = 0;
  int             err = 0;

  if (radius <= 0)
    {
      putpixel (dst, x0, y0, color);
      return 0;
    }
  while (x >= y)
    {
      hline (dst, x0 - x, x0 + x, y0 + y, color);
      hline (dst, x0 - x, x0 + x, y0 - y, color);
      hline (dst, x0 - y, x0 + y, y0 + x, color);
      hline (dst, x0 - y, x0 + y, y0 - x, color);
      y++;
      err += 1 + 2 * y;
      if (2 * (err - x) + 1 > 0)
	{
	  x--;
	  err += 1 - 2 * x;
	}
    }
  return 0;
}

int
stringColor (SDL_Surface *dst, Sint16 x, Sint16 y, const char *s,
	     Uint32 color)
{
  int             offset = 0;

  if (!fontdata)
    return -1;
  for (; *s; s++, offset += 8)
    {
      unsigned char     ch = (unsigned char) *s;
      int             row, col;

      for (row = 0; row < 8; row++)
	{
	  unsigned char     bits = fontdata[ch * 8 + row];

	  for (col = 0; col < 8; col++)
	    if (bits & (0x80 >> col))
	      putpixel (dst, x + offset + col, y + row, color);
	}
    }
  return 0;
}
