#!/bin/sh
# Debug build of maze-server on macOS (symbols, -Og). Same deps as build-macos.sh.
# Compiles objects into build/debug-obj before linking: a one-step compile+link
# deletes the temp .o files the binary's DWARF references, which would leave
# dsymutil nothing to build maze-server.dSYM from.
# Run from this directory: sh build-macos-debug.sh

set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d /opt/homebrew/lib/pkgconfig ]; then
  export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi
if [ -d /usr/local/lib/pkgconfig ]; then
  export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi

for pkg in sdl2 SDL2_net; do
  pkg-config --exists "$pkg" || {
    echo "Missing pkg-config module '$pkg'. Install dependencies, e.g.:"
    echo "  brew install sdl2 sdl2_net sdl3 pkgconf"
    exit 1
  }
done

CXX="${CXX:-clang++}"
PKG_CFLAGS="$(pkg-config --cflags sdl2 SDL2_net)"

OBJDIR="$SCRIPT_DIR/build/debug-obj"
rm -rf "$OBJDIR"
mkdir -p "$OBJDIR"
for src in *.cpp; do
  "$CXX" -c "$src" -o "$OBJDIR/${src%.cpp}.o" \
    -g -Og -Wall -pedantic -std=c++20 \
    $PKG_CFLAGS \
    -Iinclude
done

"$CXX" "$OBJDIR"/*.o -o maze-server \
  -g \
  $(pkg-config --libs sdl2 SDL2_net) \
  -lz -pthread

rm -rf maze-server.dSYM
dsymutil maze-server -o maze-server.dSYM

echo "Built (debug): $SCRIPT_DIR/maze-server (+ maze-server.dSYM)"
