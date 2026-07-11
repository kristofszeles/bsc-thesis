# Maze Game — Web Port (Emscripten / WebAssembly)

This directory builds the maze-game client for the browser. It shares the C++
sources in [`../maze-game`](../maze-game) (the same way the Android port under
[`../android`](../android) does) and compiles them to WebAssembly with
[Emscripten](https://emscripten.org). Rendering uses the OpenGL ES 3.0 code
path (`MAZE_GLES`), which maps to **WebGL 2**.

## Building

Install Emscripten (macOS: `brew install emscripten`; elsewhere see the
[emsdk instructions](https://emscripten.org/docs/getting_started/downloads.html)),
then:

```bash
sh build-web.sh
```

The output — `index.html`, `index.js`, `index.wasm`, and `index.data` (the
packaged `textures/`, `fonts/`, and `models/` assets) — lands in `dist/`.

## Running

Browsers do not run WebAssembly from `file://` URLs, so serve `dist/` over HTTP:

```bash
sh serve.sh            # http://localhost:8000/
```

Any static file server works; no special headers are required (the build is
single-threaded and does not need SharedArrayBuffer).

## Multiplayer

Browsers cannot open raw TCP sockets, so the web client connects over a
**WebSocket**. `maze-server` accepts WebSocket clients natively on its normal
game port (it detects the HTTP upgrade handshake and speaks RFC 6455 framing
for those connections — see `maze-server/websocket.h`), so no bridge or proxy
is needed:

```bash
# Terminal 1: the game server (TCP + WebSocket on the same port, default 9999)
cd ../maze-server && ./maze-server

# Browser: Multiplayer -> enter the server address, e.g.  localhost:9999
```

Native desktop clients connect to the very same `localhost:9999`; web and
native players share the same game. Note: if the page is served over HTTPS the
browser requires `wss://`, which the plain server does not terminate — serve
the game over plain HTTP for LAN play, or put a TLS-terminating proxy in front
of the server.

## How the port works

| Concern | Solution |
|---|---|
| Main loop | The native blocking loops (game, editor) are kept and built with **Asyncify**; after every `SDL_GL_SwapWindow` the wasm suspends (`maze_web::frameYield`), letting the browser present the frame, and paces itself to ~60 FPS. |
| Rendering | OpenGL ES 3.0 (`#version 300 es` shaders, the Android GLES path) on a WebGL 2 context. |
| Networking | `SDL_net` is replaced by the browser WebSocket API (`emscripten/websocket.h`) in `client.cpp`. There is no receiver thread; `Client::pump()` drains messages once per frame. |
| Config & saves | `game-config.json` and `last.map` live under `/persistent`, an IDBFS mount flushed to IndexedDB after every save — "Continue Game" and the high score survive reloads. |
| Custom maps | The native file dialogs (NFD) are replaced by browser dialogs (`maze-game/web_support.cpp`): a file input for opening, and a File System Access API save dialog for saving (download fallback on Firefox/Safari), reusing the async picker flow written for Android. |
| Assets | `textures/`, `fonts/`, `models/` are packaged into `index.data` by `--preload-file` and read through the normal filesystem code. |

## Browser notes

- Click the canvas first so it has keyboard focus.
- **Right click** captures the mouse via the Pointer Lock API. Pressing
  **Escape** releases the cursor and exits to the main menu in one press: the
  browser reserves the Escape key while the pointer is locked, so the game
  detects the lock being dropped and replays it as an Escape keypress
  (`maze_web::setPointerLockIntent`).
- **F11**: browsers own fullscreen — use the browser's fullscreen instead.
- Saving a map from the editor opens a save dialog in Chromium-based browsers
  (File System Access API). Firefox and Safari do not support that API, so the
  game asks for a filename and downloads the file; to also choose the folder,
  enable "Always ask you where to save files" in Firefox's download settings.
- "Quit Game" stops the WebAssembly runtime; reload the page to restart.
- Tested against WebGL 2; on very old browsers without WebGL 2 the game will
  not start.
