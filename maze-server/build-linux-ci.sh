#!/bin/sh
# Build maze-server on Linux using system SDL2 (CI); uses default g++ if g++-10 is absent.
# Run from this directory: sh build-linux-ci.sh

set -e
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

for pkg in sdl2 SDL2_net; do
  pkg-config --exists "$pkg" || {
    echo "Missing pkg-config module '$pkg'. Install build dependencies (see README)."
    exit 1
  }
done

CXX="${CXX:-g++}"

"$CXX" *.cpp -o maze-server \
  -O3 -Wall -pedantic -std=c++20 -no-pie \
  -Iinclude \
  $(pkg-config --cflags sdl2 SDL2_net) \
  $(pkg-config --libs sdl2 SDL2_net) \
  -lz -pthread

echo "Built: $SCRIPT_DIR/maze-server"
