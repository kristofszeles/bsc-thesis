#!/bin/sh
# Build debug .debs: separate packages so they can coexist with release bsc-thesis-maze-* debs.
# Usage: sh scripts/ci/linux/build-deb-debug.sh <version-label>
# Outputs: bsc-thesis-maze-game-debug_<ver>_<arch>.deb, bsc-thesis-maze-server-debug_<ver>_<arch>.deb

set -e
VERSION_LABEL="$1"
if [ -z "$VERSION_LABEL" ]; then
  echo "usage: $0 <version-label>" >&2
  exit 1
fi

if [ -n "${GITHUB_WORKSPACE:-}" ]; then
  ROOT="$GITHUB_WORKSPACE"
else
  SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
  ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
fi
cd "$ROOT" || exit 1

if [ ! -f maze-game/maze-game ] || [ ! -f maze-server/maze-server ]; then
  echo "error: maze-game/maze-game and maze-server/maze-server must exist (run Linux debug build first)." >&2
  echo "  cwd=$ROOT" >&2
  exit 1
fi

ARCH="$(dpkg --print-architecture)"
VER_STRIPPED="${VERSION_LABEL#v}"
DEB_VER="$VER_STRIPPED"
case "$DEB_VER" in
  *[!0-9.]* | "" | *..* | . | *.)
    HEX="$(printf '%s' "$VERSION_LABEL" | tr -cd 'a-f0-9' | head -c12)"
    DEB_VER="0.0.0~ci${HEX:-0}"
    ;;
esac

DPKG_BUILD() {
  if command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build "$1" "$2"
  else
    dpkg-deb --build "$1" "$2"
  fi
}

# --- Game client (debug) ---
PKG_GAME="bsc-thesis-maze-game-debug"
STAGE_GAME="$(mktemp -d "${TMPDIR:-/tmp}/bsc-thesis-deb-game-dbg.XXXXXX")"
mkdir -p "$STAGE_GAME/DEBIAN"
mkdir -p "$STAGE_GAME/usr/lib/${PKG_GAME}"
mkdir -p "$STAGE_GAME/usr/bin"
mkdir -p "$STAGE_GAME/usr/share/${PKG_GAME}"
mkdir -p "$STAGE_GAME/usr/share/applications"
mkdir -p "$STAGE_GAME/usr/share/doc/${PKG_GAME}"

cp maze-game/maze-game "$STAGE_GAME/usr/lib/${PKG_GAME}/maze-game"
chmod 755 "$STAGE_GAME/usr/lib/${PKG_GAME}/maze-game"
cp -R maze-game/textures maze-game/fonts maze-game/models "$STAGE_GAME/usr/share/${PKG_GAME}/"

cat > "$STAGE_GAME/usr/bin/maze-game-debug" <<EOF
#!/bin/sh
cd /usr/share/${PKG_GAME} || exit 1
exec /usr/lib/${PKG_GAME}/maze-game "\$@"
EOF
chmod 755 "$STAGE_GAME/usr/bin/maze-game-debug"

cat > "$STAGE_GAME/usr/share/applications/${PKG_GAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Maze Game (debug)
Comment=3D maze game — debug build with symbols (BSc thesis)
Exec=maze-game-debug %F
Icon=/usr/share/${PKG_GAME}/textures/bg.png
Terminal=false
Categories=Game;
EOF

cat > "$STAGE_GAME/usr/share/doc/${PKG_GAME}/copyright" <<'EOF'
Packaged from the BSc thesis maze project. Debug build (-g); binaries are not stripped.
Upstream licensing applies to third-party headers and assets; see the repository.
EOF

INST_GAME="$(du -sk "$STAGE_GAME/usr" | cut -f1)"

cat > "$STAGE_GAME/DEBIAN/control" <<EOF
Package: ${PKG_GAME}
Version: ${DEB_VER}
Section: games
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INST_GAME}
Maintainer: BSc thesis build <none@localhost>
Description: 3D multiplayer maze game — client, debug build (BSc thesis)
 OpenGL/SDL client compiled with debug symbols. Installable alongside the
 non-debug bsc-thesis-maze-game package; run maze-game-debug.
Depends: libgl1, libsdl2-2.0-0, libsdl2-image-2.0-0, libsdl2-net-2.0-0, libglew2.2 | libglew2.1 | libglew1.13, libglu1-mesa | libglu1, libgtk-3-0, libwayland-client0, zlib1g
EOF

DEB_GAME="${ROOT}/${PKG_GAME}_${DEB_VER}_${ARCH}.deb"
rm -f "$DEB_GAME"
DPKG_BUILD "$STAGE_GAME" "$DEB_GAME"
rm -rf "$STAGE_GAME"
echo "Created $DEB_GAME"

# --- Server (debug) ---
PKG_SERVER="bsc-thesis-maze-server-debug"
STAGE_SRV="$(mktemp -d "${TMPDIR:-/tmp}/bsc-thesis-deb-srv-dbg.XXXXXX")"
mkdir -p "$STAGE_SRV/DEBIAN"
mkdir -p "$STAGE_SRV/usr/lib/${PKG_SERVER}"
mkdir -p "$STAGE_SRV/usr/bin"
mkdir -p "$STAGE_SRV/usr/share/doc/${PKG_SERVER}"

cp maze-server/maze-server "$STAGE_SRV/usr/lib/${PKG_SERVER}/maze-server"
chmod 755 "$STAGE_SRV/usr/lib/${PKG_SERVER}/maze-server"

cat > "$STAGE_SRV/usr/bin/maze-server-debug" <<EOF
#!/bin/sh
STATE="\${XDG_DATA_HOME:-\$HOME/.local/share}/maze-server-debug"
mkdir -p "\$STATE"
cd "\$STATE" || exit 1
exec /usr/lib/${PKG_SERVER}/maze-server "\$@"
EOF
chmod 755 "$STAGE_SRV/usr/bin/maze-server-debug"

cat > "$STAGE_SRV/usr/share/doc/${PKG_SERVER}/copyright" <<'EOF'
Packaged from the BSc thesis maze project. Debug build (-g); binaries are not stripped.
Upstream licensing applies to third-party headers and assets; see the repository.
EOF

INST_SRV="$(du -sk "$STAGE_SRV/usr" | cut -f1)"

cat > "$STAGE_SRV/DEBIAN/control" <<EOF
Package: ${PKG_SERVER}
Version: ${DEB_VER}
Section: games
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INST_SRV}
Maintainer: BSc thesis build <none@localhost>
Description: 3D multiplayer maze game — server, debug build (BSc thesis)
 Multithreaded TCP game server with debug symbols. Data directory defaults to
 ~/.local/share/maze-server-debug. Run maze-server-debug.
Depends: libsdl2-2.0-0, libsdl2-net-2.0-0, zlib1g
EOF

DEB_SRV="${ROOT}/${PKG_SERVER}_${DEB_VER}_${ARCH}.deb"
rm -f "$DEB_SRV"
DPKG_BUILD "$STAGE_SRV" "$DEB_SRV"
rm -rf "$STAGE_SRV"
echo "Created $DEB_SRV"
