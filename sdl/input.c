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
 * Touch controls: an analog stick, drawn and tracked by the page.
 *
 * touchstick.js floats a stick under the player's thumb and reports its
 * deflection here, already normalised (see JOYFULL in koules.h). The
 * game reads it through TouchStick() and steers with the analog
 * joystick mode: heading follows the stick, thrust follows how far it
 * is pushed. The page owns the touches themselves -- during play it
 * keeps them from SDL, so they never turn into menu clicks.
 *--------------------------------------------------------------------*/
static int      stick_on = 0;
static int      stick_x, stick_y;

/* Nonzero while a thumb is on the stick; fills in its deflection. */
int
TouchStick (int *x, int *y)
{
  *x = stick_x;
  *y = stick_y;
  return stick_on;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE void
koules_touchstick (int active, int x, int y)
{
  stick_on = active;
  stick_x = active ? x : 0;
  stick_y = active ? y : 0;
}

/* Whether touches should steer rather than click: true in the game
 * proper, false in the menus, which are played by tapping. */
EMSCRIPTEN_KEEPALIVE int
koules_playing (void)
{
  return gamemode == GAME;
}
#endif

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
  return n_pressed > 0;
}

bool
IsPressed (int key)
{
  if (key >= 0 && key < SDL_NUM_SCANCODES && pressed[key] > 0)
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
