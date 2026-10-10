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
#
# ASYNCIFY lets the game's blocking usleep()/wait-for-keypress loops
# yield to the browser instead of wedging the tab; see wasm_compat.h.
set -e
cd "$(dirname "$0")"

# The .raw sound effects are stored as base64 text (sounds/*.raw.b64) so
# the repo stays text-only. Decode them if the binaries are missing.
for b in sounds/*.raw.b64; do
  raw="${b%.b64}"
  if [ ! -s "$raw" ]; then
    # Read from stdin: BSD base64 (macOS) rejects a positional input file.
    base64 -d < "$b" > "$raw.tmp" && mv "$raw.tmp" "$raw"
    echo "Decoded $raw."
  fi
done

SOURCES="koules.c menu.c gameplan.c font.c intro.c framebuffer.c cmap.c \
rcfiles.c objectsio.c sound_wasm.c storage_wasm.c sdl/init.c sdl/draw.c sdl/input.c \
sdl/primitives.c sdl/gfxfont.c"

# shellcheck disable=SC2086
emcc $SOURCES \
  -Isdl \
  -std=gnu89 \
  -include wasm_compat.h \
  -DHAVEUSLEEP -DNODIRECT -DSDLSUPPORT -DSOUND -DMOUSE \
  -O2 \
  -s USE_SDL=2 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ASYNCIFY=1 \
  -s ASYNCIFY_STACK_SIZE=16384 \
  --shell-file shell.html \
  -o koules.html

echo "Built koules.html -- serve this directory over HTTP to play."
