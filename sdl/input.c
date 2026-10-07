/***********************************************************
*                      K O U L E S                         *
*----------------------------------------------------------*
*  sdl/input.c input routines using SDL2                   *
*----------------------------------------------------------*
*  Ported from the SDL 1.2 backend (C)2013 Lubomir Rintel.  *
*  Keycodes became scancodes: SDL_Keycode values no longer *
*  fit a flat array (SDLK_UP is 0x40000052), so the whole  *
*  input layer — including the in-game key remapping menu  *
*  in menu.c, which round-trips through GetKey() — now     *
*  consistently uses SDL_Scancode (SDL_NUM_SCANCODES       *
*  entries). Scancodes are positional, which suits a game. *
***********************************************************/

#include <interface.h>
#include <stdbool.h>
#include <stdio.h>

#include <SDL.h>
#include "primitives.h"

#include "../koules.h"		/* for keys[][], player 1's configured keys */

int             pressed[SDL_NUM_SCANCODES];
int             n_pressed = 0;
int             last_pressed;

/*--------------------------------------------------------------------
 * Touch controls: a virtual 8-way joystick for player 1.
 *
 * A finger drag anywhere on the screen drives player 1's configured
 * movement keys (arrows by default). Joystick state lives in tjoy_keys[]
 * and is merged into IsPressed(); it never touches pressed[], so a
 * physical keyboard and touch can't clobber each other.
 *
 * Menus keep working through SDL's touch-to-mouse emulation
 * (SDL_HINT_TOUCH_MOUSE_EVENTS, enabled in initialize()).
 *--------------------------------------------------------------------*/
static int             joy_active = 0;	/* a finger is down */
static SDL_FingerID    joy_finger;	/* which one */
static int             joy_ox, joy_oy;	/* drag origin, px */
static int             joy_x, joy_y;	/* current finger pos, px */
static int             tjoy_keys[4];	/* up, down, left, right */

#define JOY_DEADZONE 16

static void
joy_update (int dx, int dy)
{
  int             want[4] = {0, 0, 0, 0};
  int             j;

  if (dx * dx + dy * dy >= JOY_DEADZONE * JOY_DEADZONE)
    {
      if (dy < -JOY_DEADZONE / 2)
	want[0] = 1;
      else if (dy > JOY_DEADZONE / 2)
	want[1] = 1;
      if (dx < -JOY_DEADZONE / 2)
	want[2] = 1;
      else if (dx > JOY_DEADZONE / 2)
	want[3] = 1;
    }
  for (j = 0; j < 4; j++)
    tjoy_keys[j] = want[j];
}

static void
joy_release (void)
{
  int             j;

  for (j = 0; j < 4; j++)
    tjoy_keys[j] = 0;
  joy_active = 0;
}

/* Draw the joystick indicator onto the presented frame. Called from
 * CopyToScreen() in draw.c after the game has been blitted. */
void
DrawTouchOverlay (VScreenType screen)
{
  if (!joy_active)
    return;
  circleColor (screen, joy_ox, joy_oy, 44, 0xffffff50);
  filledCircleColor (screen, joy_x, joy_y, 18, 0xffffff90);
}

void
UpdateInput (void)
{
  SDL_Event       event;
  int             val = -1;

  if (!SDL_PollEvent (&event))
    return;

  switch (event.type)
    {
    case SDL_FINGERDOWN:
      if (!joy_active)
	{
	  joy_active = 1;
	  joy_finger = event.tfinger.fingerId;
	  joy_ox = joy_x = (int) (event.tfinger.x * MAPWIDTH);
	  joy_oy = joy_y = (int) (event.tfinger.y * MAPHEIGHT);
	  joy_update (0, 0);
	}
      break;
    case SDL_FINGERMOTION:
      if (joy_active && event.tfinger.fingerId == joy_finger)
	{
	  joy_x = (int) (event.tfinger.x * MAPWIDTH);
	  joy_y = (int) (event.tfinger.y * MAPHEIGHT);
	  joy_update (joy_x - joy_ox, joy_y - joy_oy);
	}
      break;
    case SDL_FINGERUP:
      if (joy_active && event.tfinger.fingerId == joy_finger)
	joy_release ();
      break;
    case SDL_KEYDOWN:
      /* SDL2 delivers repeat events for held keys; the original SDL 1.2
       * backend never saw them (repeat off by default). Ignore them so
       * press counts stay balanced. */
      if (event.key.repeat)
	return;
      val = 1;
      last_pressed = event.key.keysym.scancode;
      break;
    case SDL_KEYUP:
      val = 0;
      last_pressed = 0;
      break;
    case SDL_MOUSEBUTTONDOWN:
      n_pressed++;
      break;
    case SDL_MOUSEBUTTONUP:
      n_pressed--;
      break;
    default:
      return;
    }

  if (val >= 0)
    {
      n_pressed += val ? 1 : -1;
      pressed[event.key.keysym.scancode] = val;
    }
}

int
GetKey (void)
{
  int             key = last_pressed;

  last_pressed = 0;
  return key;
}

bool
Pressed (void)
{
  int             j;

  if (n_pressed != 0)
    return true;
  for (j = 0; j < 4; j++)
    if (tjoy_keys[j])
      return true;
  return false;
}

bool
IsPressed (int key)
{
  int             j;

  if (key >= 0 && key < SDL_NUM_SCANCODES && pressed[key] > 0)
    return true;
  /* Merge the virtual joystick: it drives player 1's configured
   * movement keys. Kept separate from pressed[] so touch and a
   * physical keyboard can never clobber each other. */
  for (j = 0; j < 4; j++)
    if (key == keys[0][j] && tjoy_keys[j])
      return true;
  return false;
}

int
IsPressedUp (void)
{
  return pressed[SDL_SCANCODE_UP];
}

int
IsPressedDown (void)
{
  return pressed[SDL_SCANCODE_DOWN];
}

int
IsPressedLeft (void)
{
  return pressed[SDL_SCANCODE_LEFT];
}

int
IsPressedRight (void)
{
  return pressed[SDL_SCANCODE_RIGHT];
}

int
IsPressedEnter (void)
{
  return pressed[SDL_SCANCODE_RETURN] || pressed[SDL_SCANCODE_KP_ENTER];
}

int
IsPressedEsc (void)
{
  return pressed[SDL_SCANCODE_ESCAPE];
}

int
IsPressedH (void)
{
  return pressed[SDL_SCANCODE_H];
}

int
IsPressedP (void)
{
  return pressed[SDL_SCANCODE_P];
}

#ifdef MOUSE
int
MouseX (void)
{
  int             x;
  SDL_GetMouseState (&x, NULL);
  return x;
}

int
MouseY (void)
{
  int             y;
  SDL_GetMouseState (NULL, &y);
  return y;
}

int
MouseButtons (void)
{
  return SDL_GetMouseState (NULL, NULL);
}
#endif
