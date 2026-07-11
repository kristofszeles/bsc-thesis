#!/bin/sh
# Build maze-server on macOS using Homebrew SDL2 / SDL2_net.
# Run from this directory: sh build-macos.sh

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
"$CXX" *.cpp -o maze-server \
  -O3 -Wall -pedantic -std=c++20 \
  $(pkg-config --cflags sdl2 SDL2_net) \
  -Iinclude \
  $(pkg-config --libs sdl2 SDL2_net) \
  -lz -pthread

echo "Built: $SCRIPT_DIR/maze-server"
