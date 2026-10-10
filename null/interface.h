/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  null/interface.h  headless backend for the dedicated    *
*                    server                                *
***********************************************************/
/* The dedicated server runs the simulation and talks to clients; it
 * never draws anything. gameplan.c makes that explicit --
 *
 *     if (!server) effect (nos); else SEffect (lastlevel, nos);
 *
 * -- the cutscenes are played by the clients, not here. So the server
 * needs the backend's *shape*, not its behaviour.
 *
 * Linking SDL for that is pure cost: a megabyte of rendering code that
 * never executes, and, under Cloudflare Workers, Emscripten's html5
 * library dereferencing `document` at module scope in a runtime that
 * has no DOM. This header is sdl/interface.h with the SDL types
 * replaced by opaque pointers; null/backend.c stubs the functions.
 */

#ifndef _KOULES_NULL_INTERFACE_H
#define _KOULES_NULL_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

/* SDL spellings the rest of the game uses for plain integers. */
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;

/* Surfaces exist only so the game can pass them around. Nothing here
 * ever dereferences one. */
typedef void   *BitmapType;
typedef void   *VScreenType;
typedef void   *RawBitmapType;

#define COLORS 256
typedef struct
{
  struct
  {
    Uint8           red;
    Uint8           green;
    Uint8           blue;
  }
  color[COLORS];
}
Palette;

extern VScreenType background;
extern VScreenType backscreen;
extern VScreenType starbackground;

extern int      GAMEWIDTH;
extern int      GAMEHEIGHT;
extern int      MAPWIDTH;
extern int      MAPHEIGHT;
extern int      DIV;

#define EYE_RADIUS (DIV==1?5:6)
#define MOUSE_RADIUS 4

RawBitmapType   CreateBitmap (const int, const int);
BitmapType      CompileBitmap (const int, const int, const RawBitmapType);
void            ClearScreen (void);
void            SetScreen (VScreenType screen);
void            CopyToScreen (VScreenType);
void            CopyVSToVS (VScreenType, VScreenType);

void            UpdateInput (void);
int             GetKey (void);
bool            Pressed (void);
bool            IsPressed (int);
void            ClearKey (int);
void            ClearKeys (void);
void            DrawTouchOverlay (VScreenType);
int             IsPressedDown (void);
int             IsPressedEnter (void);
int             IsPressedEsc (void);
int             IsPressedH (void);
int             IsPressedLeft (void);
int             IsPressedP (void);
int             IsPressedRight (void);
int             IsPressedUp (void);

void            BSetPixel (RawBitmapType bitmap, int, int, int);
void            SMySetPixel (VScreenType, int, int, int);
int             SGetPixel (int, int);
void            SPutPixel (int, int, int);
void            SSetPixel (int, int, int);
void            DrawText (int, int, char *);
void            DrawBlackMaskedText (int, int, char *);
void            DrawWhiteMaskedText (int, int, char *);
void            DrawRectangle (int, int, int, int, int);
void            HLine (int, int, int, int);
void            Line (int, int, int, int, int);
void            Line1 (int, int, int, int, int);
void            PutBitmap (const int, const int, const int, const int,
			   const BitmapType);

void            EnableClipping (void);
void            DisableClipping (void);

void            WaitRetrace (void);
void            SetPalette (Palette * pal);

void            myusleep (unsigned long);
void            uninitialize (void);

void            fadeout (void);
void            fadein (void);
void            fadein1 (void);

#ifdef MOUSE
int             MouseX (void);
int             MouseY (void);
int             MouseButtons (void);
#endif

#endif /* _KOULES_NULL_INTERFACE_H */
