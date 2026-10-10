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

# Both builds include client.c and server.c: koules.c calls into each of
# them from its NETSUPPORT paths, so they link together regardless of
# which role the binary ends up playing at runtime.
SOURCES="koules.c menu.c gameplan.c font.c intro.c framebuffer.c cmap.c \
rcfiles.c objectsio.c sound_wasm.c storage_wasm.c client.c server.c sock_ws.c \
sdl/init.c sdl/draw.c sdl/input.c sdl/primitives.c sdl/gfxfont.c"

COMMON="-Isdl -std=gnu89 -include wasm_compat.h \
-DHAVEUSLEEP -DNODIRECT -DSDLSUPPORT -DSOUND -DMOUSE -DNETSUPPORT \
-O2 -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY=1 \
-s ASYNCIFY_STACK_SIZE=32768"

echo "Building browser client..."
# shellcheck disable=SC2086
emcc $SOURCES $COMMON \
  --shell-file shell-net.html \
  -o koules-net.html

echo "Building dedicated server..."
# The server never opens a window -- main() runs init_server()/
# server_loop() and never returns -- so SDL links but is never touched.
# shellcheck disable=SC2086
emcc $SOURCES $COMMON \
  -s ENVIRONMENT=node \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createKoules \
  -o net/koules-server.js

echo
echo "Built. To play:"
echo "  node net/server.cjs --port 8200"
echo "  python3 -m http.server 8080   # then open:"
echo "  http://localhost:8080/koules-net.html?server=ws://localhost:8200"
