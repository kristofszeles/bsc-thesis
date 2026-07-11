#pragma once

#if defined(__EMSCRIPTEN__)

#include <string>

// Browser glue for the Emscripten/WebAssembly port. Mirrors the Android
// SAF bridge in android_picker.h where the concepts overlap so game.cpp and
// editor.cpp can alias either namespace as maze_picker.
//
// The whole port is single-threaded and built with -sASYNCIFY: functions
// documented as "blocking" suspend the wasm via emscripten_sleep, which lets
// the browser run its event loop (rendering, timers, WebSocket and FileReader
// callbacks) while the C++ call stack waits.
namespace maze_web {
    // Mount an IDBFS filesystem at /persistent and load its contents from
    // IndexedDB. Blocking; call once at startup before anything reads config.
    void initPersistentFS();
    // Flush MEMFS -> IndexedDB (fire and forget). Call after writing files
    // under /persistent so they survive the tab closing.
    void persistFS();

    // Present the current GL frame and pace the main loop to ~60 Hz.
    // Call after SDL_GL_SwapWindow in every render loop.
    void frameYield();
    // Present the current GL frame without pacing (loading screens drawn
    // outside the regular loop would otherwise never reach the canvas).
    void presentFrame();

    // Async .map open/save, same contract as maze_android in android_picker.h:
    //   - launchOpenMapPicker(): opens the browser file chooser and returns
    //     immediately; bytes arrive later and are drained via consumePickedMap().
    //   - launchSaveMapPicker(): opens a save dialog (File System Access API)
    //     where the user picks the destination; browsers without that API
    //     fall back to downloading the bytes as "custom.map".
    //   - consumePickedMap():    non-blocking poll for the picked file's bytes.
    void launchOpenMapPicker();
    void launchSaveMapPicker(const std::string& data);
    bool consumePickedMap(std::string& out);

    // Browser window inner size in CSS pixels (the canvas fills the window).
    int browserWindowWidth();
    int browserWindowHeight();

    // Tell the web layer whether the game currently wants the cursor grabbed
    // (call from Game::setRelativeMouseMode). While the pointer is locked the
    // browser reserves the Escape key: pressing it exits pointer lock without
    // delivering any key event to the page. When the lock is lost while still
    // wanted, a synthetic SDL Escape keypress is pushed so a single press both
    // releases the cursor and exits to the menu, like on desktop.
    void setPointerLockIntent(bool locked);
}

#endif
