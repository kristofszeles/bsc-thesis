#!/bin/sh
# Build Maze Game.app + Maze Server.app and a compressed DMG (run from repo root).
# Prerequisites: maze-game/maze-game and maze-server/maze-server already built;
#                 dylibbundler on PATH; repo assets present under maze-game/.
# Usage: sh scripts/ci/macos-create-dmg.sh <version-label>

set -e
VERSION="$1"
if [ -z "$VERSION" ]; then
  echo "usage: $0 <version-label>" >&2
  exit 1
fi

# CFBundleShortVersionString must be major.minor.patch (no leading v)
SHORT_VER="${VERSION#v}"
case "$SHORT_VER" in
  *[!0-9.]*|""|*..*|.*|*.) SHORT_VER="1.0.0" ;;
esac

ROOT="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

GAME_APP="Maze Game.app"
SERVER_APP="Maze Server.app"
PLIST_GAME="$ROOT/scripts/ci/mac/Info-MazeGame.plist"
PLIST_SERVER="$ROOT/scripts/ci/mac/Info-MazeServer.plist"

rm -rf "$GAME_APP" "$SERVER_APP"
mkdir -p "$GAME_APP/Contents/MacOS" "$GAME_APP/Contents/Resources"
mkdir -p "$SERVER_APP/Contents/MacOS" "$SERVER_APP/Contents/Resources"

cp "$PLIST_GAME" "$GAME_APP/Contents/Info.plist"
cp "$PLIST_SERVER" "$SERVER_APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $SHORT_VER" "$GAME_APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $SHORT_VER" "$SERVER_APP/Contents/Info.plist"

cp maze-game/maze-game "$GAME_APP/Contents/MacOS/maze-game"
chmod +x "$GAME_APP/Contents/MacOS/maze-game"
cp -R maze-game/textures maze-game/fonts maze-game/models "$GAME_APP/Contents/Resources/"

cp maze-server/maze-server "$SERVER_APP/Contents/MacOS/maze-server"
chmod +x "$SERVER_APP/Contents/MacOS/maze-server"

bundle_libs() {
  APP="$1"
  EXE="$2"
  echo "Bundling libraries for $APP ($EXE)..."
  dylibbundler -od -b -x "$APP/Contents/MacOS/$EXE" -d "$APP/Contents/libs" \
    -p "@executable_path/../libs/" \
    -s /opt/homebrew/lib -s /usr/local/lib
}

bundle_libs "$GAME_APP" "maze-game"
bundle_libs "$SERVER_APP" "maze-server"

# Ad-hoc sign so Gatekeeper is less likely to reject the bundle outright
if command -v codesign >/dev/null 2>&1; then
  codesign --force --deep --sign - "$GAME_APP" 2>/dev/null || true
  codesign --force --deep --sign - "$SERVER_APP" 2>/dev/null || true
fi

STAGE="dmg-staging"
DMG="bsc-thesis-${VERSION}-macos.dmg"
rm -rf "$STAGE" "$DMG"
mkdir -p "$STAGE"
cp -R "$GAME_APP" "$SERVER_APP" "$STAGE/"
cp "$ROOT/scripts/ci/mac/README-DMG.txt" "$STAGE/Read Me.txt"
ln -sf /Applications "$STAGE/Applications"

VOLNAME="Maze Game (${VERSION})"
hdiutil create -volname "$VOLNAME" -srcfolder "$STAGE" -format UDZO \
  -imagekey zlib-level=9 -ov "$DMG"

rm -rf "$STAGE" "$GAME_APP" "$SERVER_APP"
echo "Created $DMG"
