/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  null/backend.c  headless backend for the dedicated      *
*                  server                                  *
***********************************************************/
/* Every drawing and input entry point the game links against, doing
 * nothing. See null/interface.h for why that is sufficient: the server
 * simulates and sends state, and leaves rendering to the clients.
 *
 * These are not "not implemented yet" -- on a dedicated server there is
 * genuinely nothing to draw to and no keyboard to read. The functions
 * exist so that koules.c, menu.c, font.c and intro.c link unchanged.
 */

#include <interface.h>
#include <stddef.h>

/* The game plan, matching sdl/init.c's defaults. The -E and -W server
 * options adjust these in server_main.c, exactly as they do there. */
#define WIDTH 640
#define HEIGHT 460
int             DIV = 1;
int             MAPWIDTH = WIDTH;
int             MAPHEIGHT = HEIGHT;
int             GAMEWIDTH = WIDTH;
int             GAMEHEIGHT = HEIGHT;

/* Never dereferenced; the game only passes them around. */
VScreenType     backscreen = NULL;
VScreenType     background = NULL;
VScreenType     starbackground = NULL;

RawBitmapType
CreateBitmap (const int xv, const int yv)
{
  (void) xv, (void) yv;
  return NULL;
}

BitmapType
CompileBitmap (const int x, const int y, const RawBitmapType bitmap)
{
  (void) x, (void) y;
  return bitmap;
}

void ClearScreen (void) { }
void SetScreen (VScreenType screen) { (void) screen; }
void CopyToScreen (VScreenType s) { (void) s; }
void CopyVSToVS (VScreenType a, VScreenType b) { (void) a, (void) b; }

/* No display, so nothing ever changes and nothing is ever pressed. */
void UpdateInput (void) { }
int GetKey (void) { return 0; }
bool Pressed (void) { return false; }
bool IsPressed (int key) { (void) key; return false; }
void ClearKey (int key) { (void) key; }
void ClearKeys (void) { }
void DrawTouchOverlay (VScreenType s) { (void) s; }
int IsPressedUp (void) { return 0; }
int IsPressedDown (void) { return 0; }
int IsPressedLeft (void) { return 0; }
int IsPressedRight (void) { return 0; }
int IsPressedEnter (void) { return 0; }
int IsPressedEsc (void) { return 0; }
int IsPressedH (void) { return 0; }
int IsPressedP (void) { return 0; }

void BSetPixel (RawBitmapType b, int x, int y, int c)
{ (void) b, (void) x, (void) y, (void) c; }
void SMySetPixel (VScreenType s, int x, int y, int c)
{ (void) s, (void) x, (void) y, (void) c; }
int SGetPixel (int x, int y) { (void) x, (void) y; return 0; }
void SPutPixel (int x, int y, int c) { (void) x, (void) y, (void) c; }
void SSetPixel (int x, int y, int c) { (void) x, (void) y, (void) c; }

void DrawText (int x, int y, char *t) { (void) x, (void) y, (void) t; }
void DrawBlackMaskedText (int x, int y, char *t) { (void) x, (void) y, (void) t; }
void DrawWhiteMaskedText (int x, int y, char *t) { (void) x, (void) y, (void) t; }
void DrawRectangle (int a, int b, int c, int d, int e)
{ (void) a, (void) b, (void) c, (void) d, (void) e; }
void HLine (int a, int b, int c, int d) { (void) a, (void) b, (void) c, (void) d; }
void Line (int a, int b, int c, int d, int e)
{ (void) a, (void) b, (void) c, (void) d, (void) e; }
void Line1 (int a, int b, int c, int d, int e)
{ (void) a, (void) b, (void) c, (void) d, (void) e; }
void PutBitmap (const int x, const int y, const int xs, const int ys,
		const BitmapType b)
{ (void) x, (void) y, (void) xs, (void) ys, (void) b; }

void EnableClipping (void) { }
void DisableClipping (void) { }

void WaitRetrace (void) { }
void SetPalette (Palette * pal) { (void) pal; }

void uninitialize (void) { }

/* fadeout/fadein/fadein1 and fadedout come from cmap.c, which is built
 * into the server too: they only ramp a palette through SetPalette, so
 * they are already no-ops here without needing their own stubs.
 */

#ifdef MOUSE
int MouseX (void) { return 0; }
int MouseY (void) { return 0; }
int MouseButtons (void) { return 0; }
#endif
