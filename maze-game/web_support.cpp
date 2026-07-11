#if defined(__EMSCRIPTEN__)

#include "web_support.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <SDL.h>

namespace {
    std::string g_pickedMap;
    bool g_hasPickedMap = false;

    bool g_pointerLockWanted = false;
    bool g_pointerLockWatcherInstalled = false;

    EM_BOOL onPointerLockChange(int eventType, const EmscriptenPointerlockChangeEvent* event, void* userData) {
        (void)eventType;
        (void)userData;
        if (!event->isActive && g_pointerLockWanted) {
            // The browser dropped the lock while the game still wanted it —
            // the user pressed Escape (the page never receives that keypress).
            // Replay it as an SDL event so the normal Escape handling runs.
            g_pointerLockWanted = false;
            SDL_Event sdlEvent;
            SDL_zero(sdlEvent);
            sdlEvent.type = SDL_KEYDOWN;
            sdlEvent.key.state = SDL_PRESSED;
            sdlEvent.key.keysym.sym = SDLK_ESCAPE;
            sdlEvent.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
            SDL_PushEvent(&sdlEvent);
            sdlEvent.type = SDL_KEYUP;
            sdlEvent.key.state = SDL_RELEASED;
            SDL_PushEvent(&sdlEvent);
        }
        return EM_TRUE;
    }
}

// Called from JS (FileReader.onload below) once the user picks a file. Plain
// data copy only: it runs while the wasm may be suspended in emscripten_sleep,
// which is safe as long as it does not itself unwind.
extern "C" EMSCRIPTEN_KEEPALIVE void maze_web_picked_map(const char* data, int length) {
    g_pickedMap.assign(data, static_cast<size_t>(length));
    g_hasPickedMap = true;
}

EM_JS(void, maze_web_js_open_picker, (), {
    var input = document.getElementById('maze-web-file-input');
    if (!input) {
        input = document.createElement('input');
        input.type = 'file';
        input.id = 'maze-web-file-input';
        input.accept = '.map';
        input.style.display = 'none';
        document.body.appendChild(input);
    }
    input.value = "";
    input.onchange = function(e) {
        var file = e.target.files[0];
        if (!file) return;
        var reader = new FileReader();
        reader.onload = function() {
            var bytes = new Uint8Array(reader.result);
            var buf = _malloc(bytes.length + 1);
            HEAPU8.set(bytes, buf);
            _maze_web_picked_map(buf, bytes.length);
            _free(buf);
        };
        reader.readAsArrayBuffer(file);
    };
    input.click();
});

EM_JS(void, maze_web_js_download, (const char* data, int length), {
    // Copy out of the wasm heap immediately; the buffer is freed on return.
    var bytes = HEAPU8.slice(data, data + length);
    var blob = new Blob([bytes], { type: 'application/octet-stream' });

    // Browsers without showSaveFilePicker (Firefox, Safari) cannot open a real
    // save dialog from a page; a download is the only way to write a file. At
    // least let the user pick the filename here — the target folder is decided
    // by the browser's own download settings (Firefox can be told to always
    // ask for the location).
    var fallbackDownload = function() {
        var name = window.prompt('Save map as:', 'custom.map');
        if (name === null) return;  // cancelled
        name = name.trim();
        if (name === '') name = 'custom.map';
        if (!/\.map$/i.test(name)) name += '.map';
        var a = document.createElement('a');
        a.href = URL.createObjectURL(blob);
        a.download = name;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(function() { URL.revokeObjectURL(a.href); }, 0);
    };

    if (window.showSaveFilePicker) {
        // File System Access API (Chromium): a real save dialog where the user
        // picks the destination and filename. Relies on the transient user
        // activation from the click/keypress that triggered the save.
        window.showSaveFilePicker({
            suggestedName: 'custom.map',
            types: [{
                description: 'MAP file',
                accept: { 'application/octet-stream': ['.map'] }
            }]
        }).then(function(handle) {
            return handle.createWritable().then(function(writable) {
                return writable.write(blob).then(function() { return writable.close(); });
            });
        }).catch(function(err) {
            // AbortError means the user cancelled the dialog - do nothing.
            if (!err || err.name !== 'AbortError') {
                console.warn('maze-game: save dialog failed, downloading instead:', err);
                fallbackDownload();
            }
        });
        return;
    }
    // Browsers without the API (Firefox, Safari) get a regular download.
    fallbackDownload();
});

EM_JS(void, maze_web_js_mount_idbfs, (), {
    Module.mazeFsReady = 0;
    FS.mkdir('/persistent');
    FS.mount(IDBFS, {}, '/persistent');
    FS.syncfs(true, function(err) {
        if (err) console.error('maze-game: IDBFS load failed:', err);
        Module.mazeFsReady = err ? 2 : 1;
    });
});

EM_JS(int, maze_web_js_fs_ready, (), {
    return Module.mazeFsReady | 0;
});

EM_JS(void, maze_web_js_persist, (), {
    FS.syncfs(false, function(err) {
        if (err) console.error('maze-game: IDBFS flush failed:', err);
    });
});

EM_JS(int, maze_web_js_inner_width, (), { return window.innerWidth; });
EM_JS(int, maze_web_js_inner_height, (), { return window.innerHeight; });

// Suspend the wasm until the next animation frame (Asyncify awaits the
// promise). This is what makes the game hit the display's refresh cadence:
// emscripten_sleep pays setTimeout clamping (>=4 ms, unaligned with vsync),
// while requestAnimationFrame fires exactly once per displayed frame. On
// high-refresh displays (120 Hz+) extra animation frames are awaited until
// the ~60 FPS deadline so game speed stays as designed.
EM_ASYNC_JS(void, maze_web_js_wait_frame, (double targetIntervalMs), {
    var now = performance.now();
    if (Module.mazeNextFrame === undefined || Module.mazeNextFrame < now - targetIntervalMs) {
        Module.mazeNextFrame = now;  // first frame, or fell behind: no catch-up burst
    }
    do {
        await new Promise(function(resolve) { requestAnimationFrame(resolve); });
    } while (performance.now() < Module.mazeNextFrame - 1.5);
    Module.mazeNextFrame += targetIntervalMs;
    var after = performance.now();
    if (Module.mazeNextFrame < after) Module.mazeNextFrame = after;
});

namespace maze_web {

    void initPersistentFS() {
        maze_web_js_mount_idbfs();
        while (maze_web_js_fs_ready() == 0) {
            emscripten_sleep(10);
        }
    }

    void persistFS() {
        maze_web_js_persist();
    }

    void frameYield() {
        maze_web_js_wait_frame(1000.0 / 60.0);
    }

    void presentFrame() {
        emscripten_sleep(0);
    }

    void launchOpenMapPicker() {
        g_hasPickedMap = false;
        maze_web_js_open_picker();
    }

    void launchSaveMapPicker(const std::string& data) {
        maze_web_js_download(data.c_str(), static_cast<int>(data.size()));
    }

    bool consumePickedMap(std::string& out) {
        if (!g_hasPickedMap) return false;
        out = std::move(g_pickedMap);
        g_pickedMap.clear();
        g_hasPickedMap = false;
        return true;
    }

    int browserWindowWidth() { return maze_web_js_inner_width(); }
    int browserWindowHeight() { return maze_web_js_inner_height(); }

    void setPointerLockIntent(bool locked) {
        if (!g_pointerLockWatcherInstalled) {
            g_pointerLockWatcherInstalled = true;
            emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_FALSE, onPointerLockChange);
        }
        // Order matters for the caller: the intent must be updated before SDL
        // releases the lock, so a game-initiated release (right click, exit to
        // menu) is not mistaken for the user's Escape.
        g_pointerLockWanted = locked;
    }
}

#endif
