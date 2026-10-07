/*
 * sound_wasm.c - WebAssembly sound support for Koules.
 *
 * Replaces sound.c, which fork()ed a separate sound-server process that
 * wrote to /dev/dsp through OSS ioctls -- none of which exists in a
 * browser. The game's side of the protocol is trivial (single bytes:
 * 0-6 select an effect, -1 quits), so this module plays the 8 kHz mono
 * raw samples in sounds/ directly through SDL audio, which Emscripten
 * routes to WebAudio.
 *
 * API-compatible with sound.c: init_sound, test_sound, play_sound,
 * maybe_play_sound, sound_completed, kill_sound.
 */
#ifdef SOUND
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define NUM_SOUNDS 7

static const char *sound_files[NUM_SOUNDS] =
{
  "sounds/start.raw",
  "sounds/end.raw",
  "sounds/colize.raw",
  "sounds/destroy1.raw",
  "sounds/destroy2.raw",
  "sounds/creator1.raw",
  "sounds/creator2.raw"
};

static Uint8           *sound_data[NUM_SOUNDS];
static Uint32           sound_len[NUM_SOUNDS];
static SDL_AudioDeviceID audio_dev;
static char             sound_flags[20];	/* Sound Flag for sound 1-19 */

void
test_sound (void)
{
  /* No child process to watch anymore. */
}

#ifdef __EMSCRIPTEN__
/* Async fetch callback: store the downloaded sample. */
static void
sound_fetched (void *arg, void *data, int size)
{
  int i = (int)(intptr_t)arg;
  if (i < 0 || i >= NUM_SOUNDS || size <= 0)
    return;
  sound_data[i] = malloc (size);
  if (sound_data[i])
    {
      memcpy (sound_data[i], data, size);
      sound_len[i] = size;
    }
}

static void
sound_fetch_failed (void *arg)
{
  /* Keep silent; the game plays without this effect. */
}

/* Fetch sounds in the background after startup; the game runs fine
 * (silently) until they arrive. */
static void
fetch_sounds_async (void)
{
  int i;
  for (i = 0; i < NUM_SOUNDS; i++)
    {
      if (sound_data[i] == NULL)
	emscripten_async_wget_data (sound_files[i], (void *)(intptr_t)i,
				    sound_fetched, sound_fetch_failed);
    }
}
#endif

void
init_sound (void)
{
  SDL_AudioSpec       want, have;
  int                 i;

  /* 8 kHz unsigned 8-bit mono, matching the .raw assets and the
   * original OSS DSP setup (SNDCTL_DSP_SPEED 8010, mono). */
  SDL_zero (want);
  want.freq = 8000;
  want.format = AUDIO_U8;
  want.channels = 1;
  want.samples = 512;

  audio_dev = SDL_OpenAudioDevice (NULL, 0, &want, &have, 0);
  if (audio_dev == 0)
    {
      fprintf (stderr, "sound_wasm: couldn't open audio: %s\n",
	       SDL_GetError ());
      return;
    }

  for (i = 0; i < NUM_SOUNDS; i++)
    {
      FILE               *f = fopen (sound_files[i], "rb");
      long                len;

      if (!f)
	{
	  /* Not preloaded; try the async fetch below (Emscripten) or
	   * stay silent. */
#ifdef __EMSCRIPTEN__
	  continue;
#else
	  fprintf (stderr, "sound_wasm: couldn't open %s\n", sound_files[i]);
	  continue;
#endif
	}
      fseek (f, 0, SEEK_END);
      len = ftell (f);
      fseek (f, 0, SEEK_SET);
      sound_data[i] = malloc (len);
      if (sound_data[i] && fread (sound_data[i], 1, len, f) == (size_t) len)
	sound_len[i] = len;
      else
	{
	  free (sound_data[i]);
	  sound_data[i] = NULL;
	}
      fclose (f);
    }

#ifdef __EMSCRIPTEN__
  /* Sounds not bundled; fetch them in the background. */
  fetch_sounds_async ();
#endif

  /* Browsers start the AudioContext suspended until the first user
   * gesture; Emscripten unlocks it automatically and queued samples
   * play once it does. */
  SDL_PauseAudioDevice (audio_dev, 0);

  for (i = 0; i < 20; i++)
    sound_flags[i] = 0;
}

static void
queue_sound (int k)
{
  if (audio_dev == 0 || k < 0 || k >= NUM_SOUNDS || sound_data[k] == NULL)
    return;
  /* If the browser's AudioContext is still suspended (no user gesture
   * yet), queued samples pile up and all fire at once when it unlocks.
   * Drop new effects while more than ~0.5s of audio is backed up. */
  if (SDL_GetQueuedAudioSize (audio_dev) > 4000)
    return;
  SDL_QueueAudio (audio_dev, sound_data[k], sound_len[k]);
}

int
play_sound (int k)
{
  queue_sound (k);
  return 0;
}

void
maybe_play_sound (int k)
{
  if (k < 0 || k >= 20)
    return;
  if (sound_flags[k] & 1)
    return;

  sound_flags[k] |= 1;
  queue_sound (k);
}

void
sound_completed (int k)
{
  if (k >= 0 && k < 20)
    sound_flags[k] &= ~1;
}

void
kill_sound (void)
{
  int                 i;

  if (audio_dev)
    SDL_ClearQueuedAudio (audio_dev);
  for (i = 0; i < 20; i++)
    sound_flags[i] = 0;
}
#endif
