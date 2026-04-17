#define SDL_MAIN_HANDLED

#include <iostream>

#include "server.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void maze_macos_chdir_to_bundle_resources_if_needed(void) {
    char buf[PATH_MAX];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) != 0) {
        return;
    }
    char resolved[PATH_MAX];
    if (realpath(buf, resolved) == nullptr) {
        return;
    }
    const char *in_bundle = strstr(resolved, "/Contents/MacOS/");
    if (in_bundle == nullptr) {
        return;
    }
    size_t n = (size_t)(in_bundle - resolved);
    char respath[PATH_MAX];
    if (n + sizeof("/Contents/Resources") > sizeof(respath)) {
        return;
    }
    memcpy(respath, resolved, n);
    memcpy(respath + n, "/Contents/Resources", sizeof("/Contents/Resources"));
    (void)chdir(respath);
}
#endif

int main() {
    srand((int)time(nullptr));

#ifdef __APPLE__
    maze_macos_chdir_to_bundle_resources_if_needed();
#endif

    Server server;
    server.start();
    
    std::string line;
    do {
        std::getline(std::cin, line);
        server.handleCommand(line);
    } while (line != "stop");
    
    server.join();
    
    return 0;
}