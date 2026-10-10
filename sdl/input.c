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

/* Set when a button went down, cleared when MouseButtons () reports it.
 * The menu polls the button state once per 25Hz frame and acts on the
 * frame where it goes from down to up, so a click shorter than 40ms
 * used to fall between two polls and do nothing -- which is most
 * clicks, and every tap on a touchscreen. Latching the press makes the
 * poll see it exactly once, and the release on the frame after. */
static int      mouse_clicked = 0;

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

/* Forget every held key. Used when the window loses focus: the browser
 * stops sending us key events while another element has focus, so the
 * matching KEYUP never arrives and the ship would thrust forever. */
void
ClearKeys (void)
{
  int             j;

  for (j = 0; j < SDL_NUM_SCANCODES; j++)
    pressed[j] = 0;
  n_pressed = 0;
  last_pressed = 0;
}

/* Forget one held key, for code that has consumed a keypress and does
 * not want to see it again (ESC leaving the game would otherwise be
 * read a second time by the menu underneath). */
void
ClearKey (int key)
{
  if (key < 0 || key >= SDL_NUM_SCANCODES)
    return;
  if (pressed[key])
    {
      pressed[key] = 0;
      if (n_pressed > 0)
	n_pressed--;
    }
  if (last_pressed == key)
    last_pressed = 0;
}

void
UpdateInput (void)
{
  SDL_Event       event;
  int             sc;

  /* Drain the queue rather than taking a single event per call. The
   * game calls us once per 25Hz frame, while a browser happily
   * produces mouse-motion events several times faster; taking one at a
   * time let the queue grow without bound and input fell seconds
   * behind. Key state changes still stop the drain, one per call, so
   * that a press and its release always land in different frames --
   * the key-remapping menu reads GetKey() once a frame and detects the
   * release that way. */
  while (SDL_PollEvent (&event))
    {
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
	  /* SDL2 delivers repeat events for held keys; the original
	   * SDL 1.2 backend never saw them (repeat off by default).
	   * Skip them -- they carry no state change. */
	  if (event.key.repeat)
	    break;
	  sc = event.key.keysym.scancode;
	  if (sc < 0 || sc >= SDL_NUM_SCANCODES)
	    break;
	  last_pressed = sc;
	  /* Only count real transitions, so n_pressed can never drift
	   * out of step with pressed[] and strand Pressed() at true. */
	  if (!pressed[sc])
	    {
	      pressed[sc] = 1;
	      n_pressed++;
	    }
	  return;
	case SDL_KEYUP:
	  sc = event.key.keysym.scancode;
	  if (sc < 0 || sc >= SDL_NUM_SCANCODES)
	    break;
	  last_pressed = 0;
	  if (pressed[sc])
	    {
	      pressed[sc] = 0;
	      if (n_pressed > 0)
		n_pressed--;
	    }
	  return;
	case SDL_MOUSEBUTTONDOWN:
	  n_pressed++;
	  mouse_clicked = 1;
	  break;
	case SDL_MOUSEBUTTONUP:
	  if (n_pressed > 0)
	    n_pressed--;
	  break;
	case SDL_WINDOWEVENT:
	  if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
	    {
	      ClearKeys ();
	      joy_release ();
	    }
	  break;
	default:
	  break;
	}
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

  if (n_pressed > 0)
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
  int             live = SDL_GetMouseState (NULL, NULL);

  if (live)
    {
      mouse_clicked = 0;
      return live;
    }
  if (mouse_clicked)
    {
      mouse_clicked = 0;
      return SDL_BUTTON_LMASK;
    }
  return 0;
}
#endif
