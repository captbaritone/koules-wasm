/*
 * storage_wasm.c - persist the save files across page loads.
 *
 * The game keeps its settings and unlocked level in two small binary
 * files under $HOME (see rcfiles.c). Under Emscripten $HOME lives in
 * MEMFS, which is built fresh on every page load, so every visit
 * started over with default controls at level 1.
 *
 * These two calls copy a save file between MEMFS and localStorage,
 * keyed by its path. rcfiles.c pulls a file in before reading it and
 * pushes it back out after writing it, so the on-disk format and all
 * of the save/load logic stay exactly as they are.
 *
 * localStorage can be unavailable or full (private windows, storage
 * turned off), and the saves are a convenience rather than something
 * the game needs, so every failure here is silent -- the game just
 * behaves as it did before, starting from defaults.
 */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>

/* localStorage holds strings, so the bytes go through base64. The
 * files are well under a hundred bytes each. */

EM_JS (void, koules_storage_sync_in, (const char *path),
{
  try
    {
      var p = UTF8ToString (path);
      var v = localStorage.getItem ('koules:' + p);
      if (v === null)
	return;
      var s = atob (v);
      var d = new Uint8Array (s.length);
      for (var i = 0; i < s.length; i++)
	d[i] = s.charCodeAt (i);
      FS.writeFile (p, d);
    }
  catch (e)
    {
      /* No storage, or nothing saved yet: fall back to defaults. */
    }
});

EM_JS (void, koules_storage_sync_out, (const char *path),
{
  try
    {
      var p = UTF8ToString (path);
      var d = FS.readFile (p);
      var s = String();
      for (var i = 0; i < d.length; i++)
	s += String.fromCharCode (d[i]);
      localStorage.setItem ('koules:' + p, btoa (s));
    }
  catch (e)
    {
      /* Storage full or unavailable; the save is best-effort. */
    }
});

#endif /* __EMSCRIPTEN__ */
