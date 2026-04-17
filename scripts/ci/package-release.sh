#!/bin/sh
# Package maze-game and maze-server for distribution (run from repo root).
# Usage: sh scripts/ci/package-release.sh <linux> <version-label>

set -e
PLATFORM="$1"
VERSION="$2"
if [ -z "$PLATFORM" ] || [ -z "$VERSION" ]; then
  echo "usage: $0 <linux> <version-label>" >&2
  exit 1
fi

ROOT="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

case "$PLATFORM" in
  linux)
    NAME="bsc-thesis-${VERSION}-linux-x86_64"
    rm -rf "$NAME"
    mkdir -p "$NAME/maze-game" "$NAME/maze-server"
    cp maze-game/maze-game "$NAME/maze-game/"
    cp -R maze-game/textures maze-game/fonts maze-game/models "$NAME/maze-game/"
    cp maze-server/maze-server "$NAME/maze-server/"
    tar -czf "${NAME}.tar.gz" "$NAME"
    rm -rf "$NAME"
    echo "Created ${NAME}.tar.gz"
    ;;
  *)
    echo "unknown platform: $PLATFORM" >&2
    exit 1
    ;;
esac
