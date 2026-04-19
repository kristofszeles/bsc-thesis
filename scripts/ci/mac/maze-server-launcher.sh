#!/bin/bash
# Finder-launched entry point for Maze Server.app: open Terminal running maze-server.
# GUI app launches have no TTY; the real binary stays as maze-server next to this script.
set -e
DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
SERVER="$DIR/maze-server"
if [ ! -x "$SERVER" ]; then
  osascript -e 'display dialog "maze-server not found in this app bundle." buttons {"OK"} default button 1 with title "Maze Server" with icon stop' \
    || true
  exit 1
fi
osascript - "$SERVER" <<'APPLESCRIPT'
on run argv
	set serverPath to item 1 of argv
	tell application "Terminal"
		activate
		do script "exec " & quoted form of serverPath
	end tell
end run
APPLESCRIPT
