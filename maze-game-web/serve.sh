#!/bin/sh
# Serve the built game locally; browsers refuse to run wasm from file:// URLs.
# Usage: sh serve.sh [port]   (default port 8000)
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
[ -f "$SCRIPT_DIR/dist/index.html" ] || {
  echo "dist/index.html not found - run: sh build-web.sh"
  exit 1
}
cd "$SCRIPT_DIR/dist"
echo "Open http://localhost:${1:-8000}/"
exec python3 -m http.server "${1:-8000}"
