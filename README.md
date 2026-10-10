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

`koules.html` is single player and local multiplayer, with sound.

## Network multiplayer (prototype)

The original game's network mode is a dedicated authoritative server:
`server.c` runs the simulation at 25Hz and sends bit-packed state
snapshots, and clients only render and send input. It speaks UDP, which
a browser cannot, so `sock_ws.c` replaces `sock.c` and carries the same
datagrams over a WebSocket. `server.c` and `client.c` are unmodified.

The server is compiled to WebAssembly and runs headless. It never draws
— it tells clients to play the cutscenes instead — so the same build
runs under Node today and could run in a Cloudflare Durable Object by
replacing only the host glue in `net/server.cjs`.

```sh
source ~/emsdk/emsdk_env.sh
./build-net.sh
(cd net && npm install)

node net/server.cjs --port 8200     # dedicated server
python3 -m http.server 8080         # then open, in two tabs:
# http://localhost:8080/koules-net.html?server=ws://localhost:8200
```

### Rooms, on Cloudflare

`cf/` runs the same server as a Durable Object, one per room. A Worker
serves the client and routes `/room/<name>` to `idFromName(name)`, which
is deterministic, so everyone who opens the same link lands in the same
game. The DO is single-threaded and globally unique for its name, so
there is exactly one simulation per room and nothing to coordinate.

```sh
cd cf && npm install && npx wrangler dev
# then open, in two tabs:
# http://127.0.0.1:8787/?room=abc
```

The server links the headless backend in `null/` rather than SDL -- it
never draws, so there is nothing to link a renderer for. That keeps the
server wasm at ~130KB instead of ~1MB, and avoids Emscripten's html5
library, which dereferences `document` at module scope in a runtime that
has no DOM.

A room locks when its game starts, as it always did; latecomers are now
told so instead of waiting on a black screen.

Each client picks REGISTER PLAYERS, then START GAME; the server starts
once every connected client has registered. Measured traffic is about
1.2 KB/s down (25 snapshots/sec, ~48 bytes each) and under 100 B/s up.

Note that the client does no prediction — it draws exactly what the
server last sent — so input lag is a full round trip, as it always was.

## License

GPL-2.0-or-later, © 1995–1998 Jan Hubicka and Kamil Toman. See COPYING.
