#!/bin/sh
# Build the networked (prototype) Koules: a browser client and a
# headless dedicated server, both from the game's own netcode.
#
# Requires the emsdk environment:
#   source ~/emsdk/emsdk_env.sh
#
# Produces:
#   koules-net.html + .js + .wasm   browser client  (shell-net.html)
#   net/koules-server.js + .wasm    dedicated server, run under Node
#
# Both link sock_ws.c instead of sock.c, which carries the game's UDP
# datagrams over a WebSocket; see the comment at the top of that file.
# server.c and client.c are built unmodified.
set -e
cd "$(dirname "$0")"

for b in sounds/*.raw.b64; do
  raw="${b%.b64}"
  if [ ! -s "$raw" ]; then
    base64 -d < "$b" > "$raw.tmp" && mv "$raw.tmp" "$raw"
  fi
done

# The game proper. Both roles include client.c and server.c: koules.c
# calls into each of them from its NETSUPPORT paths, so they link
# together regardless of which role the binary plays at runtime.
GAME="koules.c menu.c gameplan.c font.c intro.c framebuffer.c cmap.c \
rcfiles.c objectsio.c storage_wasm.c client.c server.c sock_ws.c"

# The client draws, so it gets SDL and sound.
CLIENT_SOURCES="$GAME sound_wasm.c \
sdl/init.c sdl/draw.c sdl/input.c sdl/primitives.c sdl/gfxfont.c"

# The server does not. null/ supplies a headless backend and a
# server-only main(), so SDL is not linked at all -- that drops the
# wasm from ~1MB to ~130KB and, more to the point, removes Emscripten's
# html5 library, which dereferences `document` at module scope in a
# Cloudflare Worker that has no DOM.
SERVER_SOURCES="$GAME mygetopt.c null/backend.c null/server_main.c"

COMMON="-std=gnu89 -include wasm_compat.h \
-DHAVEUSLEEP -DNODIRECT -DMOUSE -DNETSUPPORT \
-O2 -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY=1 -s ASYNCIFY_STACK_SIZE=32768"

CLIENT_FLAGS="-Isdl -DSDLSUPPORT -DSOUND -s USE_SDL=2"
SERVER_FLAGS="-Inull"

echo "Building browser client..."
# shellcheck disable=SC2086
emcc $CLIENT_SOURCES $COMMON $CLIENT_FLAGS \
  --shell-file shell-net.html \
  -o koules-net.html

echo "Building dedicated server (Node)..."
# shellcheck disable=SC2086
emcc $SERVER_SOURCES $COMMON $SERVER_FLAGS \
  -s ENVIRONMENT=node \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createKoules \
  -o net/koules-server.js

echo "Building Cloudflare Worker server..."
# Same server, as an ES module for the Workers runtime. The .wasm stays
# a separate file because Workers import it as a WebAssembly.Module and
# instantiate it synchronously (cf/src/room.js) -- Workers forbid I/O
# during startup, so it must not fetch its own binary.
mkdir -p cf/public
# shellcheck disable=SC2086
emcc $SERVER_SOURCES $COMMON $SERVER_FLAGS \
  -s ENVIRONMENT=web \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s EXPORT_NAME=createKoules \
  -o cf/koules-server.mjs

# The Worker serves the client as a static asset.
cp koules-net.html cf/public/index.html
cp koules-net.js cf/public/
cp koules-net.wasm cf/public/
mkdir -p cf/public/sounds
cp sounds/*.raw cf/public/sounds/

echo
echo "Built. To play:"
echo "  node net/server.cjs --port 8200"
echo "  python3 -m http.server 8080   # then open:"
echo "  http://localhost:8080/koules-net.html?server=ws://localhost:8200"
echo
echo "Or against a local Durable Object:"
echo "  (cd cf && npx wrangler dev)   # then open the printed URL + ?room=abc"
