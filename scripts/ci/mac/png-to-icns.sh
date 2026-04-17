#!/bin/sh
# Build a macOS .icns from a PNG using Pillow (nearest-neighbor) + iconutil (macOS only).
# Usage: sh png-to-icns.sh <source.png> <output.icns>

set -e
SRC="$1"
OUT="$2"
if [ -z "$SRC" ] || [ -z "$OUT" ]; then
  echo "usage: $0 <source.png> <output.icns>" >&2
  exit 1
fi
if [ ! -f "$SRC" ]; then
  echo "not found: $SRC" >&2
  exit 1
fi

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/nfd-icon.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT INT HUP
ICONSET_DIR="$TMP/AppIcon.iconset"
mkdir "$ICONSET_DIR"

python3 "$SCRIPT_DIR/png_to_iconset.py" "$SRC" "$ICONSET_DIR"

mkdir -p "$(dirname "$OUT")"
iconutil -c icns "$ICONSET_DIR" -o "$OUT"
