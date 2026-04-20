#!/bin/sh
# Debug build of maze-game for Linux CI (symbols, no optimization). Same deps as build-linux-ci.sh.
# Run from this directory: sh build-linux-ci-debug.sh

set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

for pkg in gtk+-3.0 sdl2 SDL2_image SDL2_net glew wayland-client; do
  pkg-config --exists "$pkg" || {
    echo "Missing pkg-config module '$pkg'. Install build dependencies (see README)."
    exit 1
  }
done

NFD_A="$SCRIPT_DIR/build/nfd/libnfd.a"
if [ ! -f "$NFD_A" ]; then
  echo "Building nativefiledialog-extended (needs git and cmake)..."
  mkdir -p "$SCRIPT_DIR/build"
  NFD_SRC="$SCRIPT_DIR/build/nfd-src"
  if [ ! -d "$NFD_SRC/.git" ]; then
    git clone --depth 1 https://github.com/btzy/nativefiledialog-extended.git "$NFD_SRC"
  fi
  (cd "$NFD_SRC" && git submodule update --init --recursive)
  cmake -S "$NFD_SRC" -B "$NFD_SRC/build" -DCMAKE_BUILD_TYPE=Release -DNFD_BUILD_TESTS=OFF
  cmake --build "$NFD_SRC/build" -j"$(nproc 2>/dev/null || echo 4)"
  mkdir -p "$SCRIPT_DIR/build/nfd"
  cp "$NFD_SRC/build/src/libnfd.a" "$NFD_A"
fi

CXX="${CXX:-g++}"
"$CXX" *.cpp -o maze-game \
  -g -Og -Wall -pedantic -std=c++20 -no-pie \
  -Iinclude \
  -L"$SCRIPT_DIR/build/nfd" -lnfd \
  $(pkg-config --libs wayland-client) \
  $(pkg-config --cflags gtk+-3.0 sdl2 SDL2_image SDL2_net glew) \
  $(pkg-config --libs gtk+-3.0 sdl2 SDL2_image SDL2_net glew) \
  -lGL -lGLU -lz -pthread

echo "Built (debug): $SCRIPT_DIR/maze-game"
