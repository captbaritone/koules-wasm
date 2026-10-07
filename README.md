# Koules for the Web

[Koules](https://github.com/lkundrak/koules) — the 1995 arcade game by Jan Hubicka — ported to WebAssembly. Play it in your browser.

**[▶ Play now](https://captbaritone.github.io/koules-wasm/)**

## Controls

- **Arrow keys** — steer your ship
- **Enter** — confirm menu selections
- **P** — pause, **H** — help, **Esc** — back to menu
- **Touch** — drag anywhere for a virtual joystick (player 1); tap for menu clicks

## Building

Requires the [Emscripten SDK](https://emscripten.org/):

```sh
source ~/emsdk/emsdk_env.sh
./build-wasm.sh
# serve this directory over HTTP and open koules.html
```

Network multiplayer is compiled out (browsers can't open raw TCP sockets);
single player, local multiplayer, and sound are all in.

## License

GPL-2.0-or-later, © 1995–1998 Jan Hubicka and Kamil Toman. See COPYING.
