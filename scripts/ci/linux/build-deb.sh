#!/bin/sh
# Build two installable .debs: maze-game client and maze-server (run from repo root).
# Usage: sh scripts/ci/linux/build-deb.sh <version-label>
# Outputs: bsc-thesis-maze-game_<ver>_<arch>.deb, bsc-thesis-maze-server_<ver>_<arch>.deb

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
  echo "error: maze-game/maze-game and maze-server/maze-server must exist (run Linux build first)." >&2
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

# --- Game client ---
PKG_GAME="bsc-thesis-maze-game"
STAGE_GAME="$(mktemp -d "${TMPDIR:-/tmp}/bsc-thesis-deb-game.XXXXXX")"
mkdir -p "$STAGE_GAME/DEBIAN"
mkdir -p "$STAGE_GAME/usr/lib/${PKG_GAME}"
mkdir -p "$STAGE_GAME/usr/bin"
mkdir -p "$STAGE_GAME/usr/share/${PKG_GAME}"
mkdir -p "$STAGE_GAME/usr/share/applications"
mkdir -p "$STAGE_GAME/usr/share/doc/${PKG_GAME}"

cp maze-game/maze-game "$STAGE_GAME/usr/lib/${PKG_GAME}/maze-game"
chmod 755 "$STAGE_GAME/usr/lib/${PKG_GAME}/maze-game"
cp -R maze-game/textures maze-game/fonts maze-game/models "$STAGE_GAME/usr/share/${PKG_GAME}/"

cat > "$STAGE_GAME/usr/bin/maze-game" <<EOF
#!/bin/sh
cd /usr/share/${PKG_GAME} || exit 1
exec /usr/lib/${PKG_GAME}/maze-game "\$@"
EOF
chmod 755 "$STAGE_GAME/usr/bin/maze-game"

cat > "$STAGE_GAME/usr/share/applications/${PKG_GAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Maze Game
Comment=3D maze game (BSc thesis)
Exec=maze-game %F
Icon=/usr/share/${PKG_GAME}/textures/bg.png
Terminal=false
Categories=Game;
EOF

cat > "$STAGE_GAME/usr/share/doc/${PKG_GAME}/copyright" <<'EOF'
Packaged from the BSc thesis maze project. Upstream licensing applies to
third-party headers and assets bundled in the source tree; see the repository.
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
Description: 3D multiplayer maze game — client (BSc thesis)
 OpenGL/SDL client; install bsc-thesis-maze-server on another host for multiplayer.
Depends: libgl1, libsdl2-2.0-0, libsdl2-image-2.0-0, libsdl2-net-2.0-0, libglew2.2 | libglew2.1 | libglew1.13, libglu1-mesa | libglu1, libgtk-3-0, libwayland-client0, zlib1g
EOF

DEB_GAME="${ROOT}/${PKG_GAME}_${DEB_VER}_${ARCH}.deb"
rm -f "$DEB_GAME"
DPKG_BUILD "$STAGE_GAME" "$DEB_GAME"
rm -rf "$STAGE_GAME"
echo "Created $DEB_GAME"

# --- Server ---
PKG_SERVER="bsc-thesis-maze-server"
STAGE_SRV="$(mktemp -d "${TMPDIR:-/tmp}/bsc-thesis-deb-srv.XXXXXX")"
mkdir -p "$STAGE_SRV/DEBIAN"
mkdir -p "$STAGE_SRV/usr/lib/${PKG_SERVER}"
mkdir -p "$STAGE_SRV/usr/bin"
mkdir -p "$STAGE_SRV/usr/share/doc/${PKG_SERVER}"

cp maze-server/maze-server "$STAGE_SRV/usr/lib/${PKG_SERVER}/maze-server"
chmod 755 "$STAGE_SRV/usr/lib/${PKG_SERVER}/maze-server"

cat > "$STAGE_SRV/usr/bin/maze-server" <<EOF
#!/bin/sh
STATE="\${XDG_DATA_HOME:-\$HOME/.local/share}/maze-server"
mkdir -p "\$STATE"
cd "\$STATE" || exit 1
exec /usr/lib/${PKG_SERVER}/maze-server "\$@"
EOF
chmod 755 "$STAGE_SRV/usr/bin/maze-server"

cat > "$STAGE_SRV/usr/share/doc/${PKG_SERVER}/copyright" <<'EOF'
Packaged from the BSc thesis maze project. Upstream licensing applies to
third-party headers and assets bundled in the source tree; see the repository.
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
Description: 3D multiplayer maze game — server (BSc thesis)
 Multithreaded TCP game server. Config and logs use ~/.local/share/maze-server
 unless XDG_DATA_HOME is set.
Depends: libsdl2-2.0-0, libsdl2-net-2.0-0, zlib1g
EOF

DEB_SRV="${ROOT}/${PKG_SERVER}_${DEB_VER}_${ARCH}.deb"
rm -f "$DEB_SRV"
DPKG_BUILD "$STAGE_SRV" "$DEB_SRV"
rm -rf "$STAGE_SRV"
echo "Created $DEB_SRV"
