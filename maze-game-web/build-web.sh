#!/bin/sh
# Build the WebAssembly port of maze-game with Emscripten.
# Run from this directory: sh build-web.sh
# Requires emcc (https://emscripten.org) on PATH; output goes to dist/.

set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
MAZE_DIR="$SCRIPT_DIR/../maze-game"

command -v emcc >/dev/null 2>&1 || {
  echo "emcc not found. Install the Emscripten SDK, e.g.:"
  echo "  brew install emscripten     # macOS"
  echo "  or follow https://emscripten.org/docs/getting_started/downloads.html"
  exit 1
}

# Header-only third-party libs come from maze-game/include, but that directory
# also carries Windows copies of the SDL/GLEW/zlib headers which must not
# shadow the Emscripten ports. Expose only what the web build needs.
INC_DIR="$SCRIPT_DIR/build/include"
rm -rf "$INC_DIR"
mkdir -p "$INC_DIR"
ln -s "$MAZE_DIR/include/glm" "$INC_DIR/glm"
ln -s "$MAZE_DIR/include/nlohmann" "$INC_DIR/nlohmann"
ln -s "$MAZE_DIR/include/gzip" "$INC_DIR/gzip"
ln -s "$MAZE_DIR/include/CppThread.h" "$INC_DIR/CppThread.h"

mkdir -p "$SCRIPT_DIR/dist"

# Notes on the link settings:
#   ASYNCIFY            keeps the game's native blocking main loop (and the
#                       editor's nested loop) by suspending the wasm each frame.
#   MIN/MAX_WEBGL=2     the renderer uses the OpenGL ES 3.0 path (MAZE_GLES).
#   idbfs.js            persistent /persistent mount for game-config.json + last.map.
#   websocket.js        multiplayer transport (see run-ws-bridge.sh).
emcc \
  "$MAZE_DIR/base64.cpp" \
  "$MAZE_DIR/client.cpp" \
  "$MAZE_DIR/drawutils.cpp" \
  "$MAZE_DIR/editor.cpp" \
  "$MAZE_DIR/entity.cpp" \
  "$MAZE_DIR/game.cpp" \
  "$MAZE_DIR/main.cpp" \
  "$MAZE_DIR/map.cpp" \
  "$MAZE_DIR/maze.cpp" \
  "$MAZE_DIR/mesh.cpp" \
  "$MAZE_DIR/web_support.cpp" \
  "$MAZE_DIR/window.cpp" \
  -o "$SCRIPT_DIR/dist/index.html" \
  -std=c++20 -O3 -Wall \
  -I"$INC_DIR" \
  -sUSE_SDL=2 \
  -sUSE_SDL_IMAGE=2 \
  -sSDL2_IMAGE_FORMATS=png \
  -sUSE_ZLIB=1 \
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 \
  -sALLOW_MEMORY_GROWTH \
  -sEXPORTED_FUNCTIONS=_main,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=UTF8ToString \
  -lidbfs.js -lwebsocket.js \
  --preload-file "$MAZE_DIR/textures@/textures" \
  --preload-file "$MAZE_DIR/fonts@/fonts" \
  --preload-file "$MAZE_DIR/models@/models" \
  --shell-file "$SCRIPT_DIR/shell.html"

echo "Built: $SCRIPT_DIR/dist/index.html"
echo "Serve it with: sh serve.sh   (then open http://localhost:8000/)"
