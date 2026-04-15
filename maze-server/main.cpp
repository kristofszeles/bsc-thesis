#define SDL_MAIN_HANDLED

#include <iostream>

#include "server.h"

int main() {
    srand((int)time(nullptr));
    
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