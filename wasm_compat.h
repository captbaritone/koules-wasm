/*
 * wasm_compat.h - make the game's blocking loops browser-safe.
 *
 * Koules is written the way a 1995 game is: animation and pacing loops
 * call usleep(), and loops that wait for a keypress spin on
 * UpdateInput(). Natively that is fine. In a browser it is fatal --
 * everything runs on the page's one thread, so Emscripten's usleep()
 * busy-waits and the JS callbacks that deliver SDL events never get to
 * run. A spin loop therefore waits for an event that can never arrive,
 * and the tab is wedged for good.
 *
 * Asyncify (-sASYNCIFY, see build-wasm.sh) lets a C function suspend
 * and return to the browser's event loop, resuming where it left off.
 * Routing usleep() through emscripten_sleep() turns every one of those
 * loops back into what it was always meant to be: the page repaints,
 * input arrives, and the loop carries on. The cutscenes animate and can
 * be skipped with a keypress; pause really pauses.
 *
 * Pulled in for every translation unit via emcc's -include.
 */
#ifndef KOULES_WASM_COMPAT_H
#define KOULES_WASM_COMPAT_H

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <unistd.h>

/* usleep() takes microseconds, emscripten_sleep() milliseconds. Round
 * up so a sub-millisecond sleep still yields rather than spinning. */
#define usleep(us) emscripten_sleep (((unsigned long) (us) + 999) / 1000)

/* Loops that wait on input have no sleep of their own to borrow. One
 * frame's worth of yield keeps them responsive without burning CPU. */
#define koules_yield() emscripten_sleep (10)

/* Move a save file between MEMFS and localStorage so settings and the
 * unlocked level survive a reload; see storage_wasm.c. */
void            koules_storage_sync_in (const char *path);
void            koules_storage_sync_out (const char *path);

#else /* !__EMSCRIPTEN__ */

/* Native builds block the way they always did, and save to a real
 * home directory. */
#define koules_yield() do { } while (0)
#define koules_storage_sync_in(path) do { (void) (path); } while (0)
#define koules_storage_sync_out(path) do { (void) (path); } while (0)

#endif /* __EMSCRIPTEN__ */

#endif /* KOULES_WASM_COMPAT_H */
