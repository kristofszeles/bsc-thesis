#pragma once

#if defined(__ANDROID__)

#include <string>

// Storage Access Framework bridge to MainActivity.java for picking and writing .map files.
// All three functions are safe to call from the SDL thread.
//   - launchOpenMapPicker():  fires ACTION_OPEN_DOCUMENT; returns immediately. Bytes arrive
//                             asynchronously and are drained via consumePickedMap().
//   - launchSaveMapPicker():  fires ACTION_CREATE_DOCUMENT with the given bytes; Java writes
//                             them to whatever URI the user picks.
//   - consumePickedMap():     non-blocking poll for the most recently picked file's bytes.
namespace maze_android {
    void launchOpenMapPicker();
    void launchSaveMapPicker(const std::string& data);
    bool consumePickedMap(std::string& out);
}

#endif
