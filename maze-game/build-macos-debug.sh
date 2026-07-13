#!/bin/sh
# Debug build of maze-game on macOS (symbols, -Og). Same deps as build-macos.sh.
# Compiles objects into build/debug-obj before linking: a one-step compile+link
# deletes the temp .o files the binary's DWARF references, which would leave
# dsymutil nothing to build maze-game.dSYM from.
# Run from this directory: sh build-macos-debug.sh

set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Homebrew: Apple Silicon vs Intel (pkg-config must find *.pc files)
if [ -d /opt/homebrew/lib/pkgconfig ]; then
  export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi
if [ -d /usr/local/lib/pkgconfig ]; then
  export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi

for pkg in sdl2 SDL2_image SDL2_net glew; do
  pkg-config --exists "$pkg" || {
    echo "Missing pkg-config module '$pkg'. Install dependencies, e.g.:"
    echo "  brew install sdl2 sdl2_image sdl2_net sdl3 glew pkgconf cmake"
    exit 1
  }
done

NFD_A="$SCRIPT_DIR/build/nfd/libnfd.a"
if [ ! -f "$NFD_A" ]; then
  echo "Building nativefiledialog-extended (first run; needs git and cmake)..."
  mkdir -p "$SCRIPT_DIR/build"
  NFD_SRC="$SCRIPT_DIR/build/nfd-src"
  if [ ! -d "$NFD_SRC/.git" ]; then
    git clone --depth 1 https://github.com/btzy/nativefiledialog-extended.git "$NFD_SRC"
  fi
  cmake -S "$NFD_SRC" -B "$NFD_SRC/build" -DCMAKE_BUILD_TYPE=Release -DNFD_BUILD_TESTS=OFF
  cmake --build "$NFD_SRC/build" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  mkdir -p "$SCRIPT_DIR/build/nfd"
  cp "$NFD_SRC/build/src/libnfd.a" "$NFD_A"
fi

CXX="${CXX:-clang++}"
PKG_CFLAGS="$(pkg-config --cflags sdl2 SDL2_image SDL2_net glew)"

OBJDIR="$SCRIPT_DIR/build/debug-obj"
rm -rf "$OBJDIR"
mkdir -p "$OBJDIR"
for src in *.cpp; do
  "$CXX" -c "$src" -o "$OBJDIR/${src%.cpp}.o" \
    -g -Og -Wall -pedantic -std=c++20 \
    $PKG_CFLAGS \
    -Iinclude
done

"$CXX" "$OBJDIR"/*.o -o maze-game \
  -g \
  -L"$SCRIPT_DIR/build/nfd" -lnfd \
  $(pkg-config --libs sdl2 SDL2_image SDL2_net glew) \
  -lz -framework OpenGL -framework AppKit -framework UniformTypeIdentifiers -pthread

rm -rf maze-game.dSYM
dsymutil maze-game -o maze-game.dSYM

echo "Built (debug): $SCRIPT_DIR/maze-game (+ maze-game.dSYM)"
