#!/bin/sh
# Debug build of maze-server for Linux CI (symbols, minimal optimization). Same deps as build-linux-ci.sh.
# Run from this directory: sh build-linux-ci-debug.sh

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
  -g -Og -Wall -pedantic -std=c++20 -no-pie \
  -Iinclude \
  $(pkg-config --cflags sdl2 SDL2_net) \
  $(pkg-config --libs sdl2 SDL2_net) \
  -lz -pthread

echo "Built (debug): $SCRIPT_DIR/maze-server"
