#!/bin/sh
# Build an installable .deb from built maze-game / maze-server (run from repo root).
# Usage: sh scripts/ci/linux/build-deb.sh <version-label>
# Requires: maze-game/maze-game, maze-server/maze-server; dpkg-deb (dpkg); fakeroot recommended.

set -e
VERSION_LABEL="$1"
if [ -z "$VERSION_LABEL" ]; then
  echo "usage: $0 <version-label>" >&2
  exit 1
fi

ROOT="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

PKG="bsc-thesis-maze"
ARCH="$(dpkg --print-architecture)"
VER_STRIPPED="${VERSION_LABEL#v}"
DEB_VER="$VER_STRIPPED"
case "$DEB_VER" in
  *[!0-9.]* | "" | *..* | . | *.)
    HEX="$(printf '%s' "$VERSION_LABEL" | tr -cd 'a-f0-9' | head -c12)"
    DEB_VER="0.0.0~ci${HEX:-0}"
    ;;
esac

STAGE="$(mktemp -d "${TMPDIR:-/tmp}/bsc-thesis-deb.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT INT HUP
mkdir -p "$STAGE/DEBIAN"
mkdir -p "$STAGE/usr/lib/${PKG}"
mkdir -p "$STAGE/usr/bin"
mkdir -p "$STAGE/usr/share/${PKG}/maze-game"
mkdir -p "$STAGE/usr/share/applications"
mkdir -p "$STAGE/usr/share/doc/${PKG}"

cp maze-game/maze-game "$STAGE/usr/lib/${PKG}/maze-game"
chmod 755 "$STAGE/usr/lib/${PKG}/maze-game"
cp maze-server/maze-server "$STAGE/usr/lib/${PKG}/maze-server"
chmod 755 "$STAGE/usr/lib/${PKG}/maze-server"

cp -R maze-game/textures maze-game/fonts maze-game/models "$STAGE/usr/share/${PKG}/maze-game/"

# Client: game expects cwd next to textures/ (see README).
cat > "$STAGE/usr/bin/maze-game" <<EOF
#!/bin/sh
cd /usr/share/${PKG}/maze-game || exit 1
exec /usr/lib/${PKG}/maze-game "\$@"
EOF
chmod 755 "$STAGE/usr/bin/maze-game"

# Server: writable cwd for server-config.json (XDG data dir).
cat > "$STAGE/usr/bin/maze-server" <<EOF
#!/bin/sh
STATE="\${XDG_DATA_HOME:-\$HOME/.local/share}/maze-server"
mkdir -p "\$STATE"
cd "\$STATE" || exit 1
exec /usr/lib/${PKG}/maze-server "\$@"
EOF
chmod 755 "$STAGE/usr/bin/maze-server"

cat > "$STAGE/usr/share/applications/${PKG}-game.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Maze Game
Comment=3D maze game (BSc thesis)
Exec=maze-game %F
Icon=/usr/share/${PKG}/maze-game/textures/bg.png
Terminal=false
Categories=Game;
EOF

cat > "$STAGE/usr/share/doc/${PKG}/copyright" <<'EOF'
Packaged from the BSc thesis maze project. Upstream licensing applies to
third-party headers and assets bundled in the source tree; see the repository.
EOF

INST_KB="$(du -sk "$STAGE/usr" | cut -f1)"

cat > "$STAGE/DEBIAN/control" <<EOF
Package: ${PKG}
Version: ${DEB_VER}
Section: games
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INST_KB}
Maintainer: BSc thesis build <none@localhost>
Description: 3D multiplayer maze game (thesis)
 Standalone OpenGL/SDL client and TCP server. Run maze-game or maze-server
 from the shell after install; config files live in the working directory
 (game: under /usr/share; server: under ~/.local/share/maze-server).
Depends: libgl1, libsdl2-2.0-0, libsdl2-image-2.0-0, libsdl2-net-2.0-0, libglew2.2 | libglew2.1 | libglew1.13, libglu1-mesa | libglu1, libgtk-3-0, libwayland-client0, zlib1g
EOF

DEB_OUT="${ROOT}/${PKG}_${DEB_VER}_${ARCH}.deb"
rm -f "$DEB_OUT"

if command -v fakeroot >/dev/null 2>&1; then
  fakeroot dpkg-deb --build "$STAGE" "$DEB_OUT"
else
  dpkg-deb --build "$STAGE" "$DEB_OUT"
fi

trap - EXIT INT HUP
rm -rf "$STAGE"
echo "Created $DEB_OUT"
