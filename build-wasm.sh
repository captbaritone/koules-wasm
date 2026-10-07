#!/bin/sh
# Build Koules for WebAssembly with Emscripten.
#
# Requires the emsdk environment:
#   source ~/emsdk/emsdk_env.sh
#
# Produces koules.html + koules.js + koules.wasm + koules.data.
# Network multiplayer is compiled out (browsers can't open raw TCP
# sockets); everything else -- single player, local multiplayer,
# sound -- is in.
set -e
cd "$(dirname "$0")"

SOURCES="koules.c menu.c gameplan.c font.c intro.c framebuffer.c cmap.c \
rcfiles.c objectsio.c sound_wasm.c sdl/init.c sdl/draw.c sdl/input.c \
sdl/primitives.c sdl/gfxfont.c"

# shellcheck disable=SC2086
emcc $SOURCES \
  -Isdl \
  -std=gnu89 \
  -DHAVEUSLEEP -DNODIRECT -DSDLSUPPORT -DSOUND -DMOUSE \
  -O2 \
  -s USE_SDL=2 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file sounds \
  --shell-file shell.html \
  -o koules.html

echo "Built koules.html -- serve this directory over HTTP to play."
