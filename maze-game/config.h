#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

#if defined(__EMSCRIPTEN__)
#include "web_support.h"
#endif

using json = nlohmann::json;

class Config {
private:
    std::string fileName;
    json config;
public:
    Config(const std::string& fileName) {
        this->fileName = fileName;
        load();
    }
    ~Config() {
        save();
    }
    void init() {
        config["game"]["cameraFov"] = 60.0;
        config["game"]["renderDistance"] = 30;
        config["game"]["mouseSensitivity"] = 0.27f;
        config["game"]["vehicle"] = 0;
        config["game"]["singlePlayer"]["score"] = 0;
        config["game"]["singlePlayer"]["highScore"] = 0;
        config["game"]["singlePlayer"]["health"] = 0;
        config["game"]["singlePlayer"]["cameraMode"] = 0;
        config["game"]["singlePlayer"]["cameraYaw"] = 0.0f;
        config["game"]["singlePlayer"]["cameraPitch"] = 0.0f;
        config["window"]["defaultWidth"] = 1280;
        config["window"]["defaultHeight"] = 720;
        config["window"]["maximized"] = false;
        config["maze"]["hard"]["width"] = 40;
        config["maze"]["hard"]["height"] = 40;
        config["maze"]["medium"]["width"] = 10;
        config["maze"]["medium"]["height"] = 10;
        config["maze"]["easy"]["width"] = 5;
        config["maze"]["easy"]["height"] = 5;
        config["multiplayer"]["playerName"] = "";
        config["multiplayer"]["defaultHost"] = "localhost";
        config["multiplayer"]["defaultPort"] = 9999;
    }
    void load() {
        if (!std::filesystem::exists(fileName)) {
            init();
            save();
        }
        try {
            std::ifstream file;
            file.open(fileName);
            file >> config;
            file.close();
        }
        catch (std::exception& ex) {
            std::cout << ex.what() << std::endl;
            // recreate configuration file
            init();
            save();
        }
    }
    void save() {
        std::ofstream file;
        file.open(fileName);
        file << config.dump(2);
        file.close();
#if defined(__EMSCRIPTEN__)
        // The file lives on an in-memory FS; flush it to IndexedDB so the
        // config survives the tab closing.
        maze_web::persistFS();
#endif
    }
    json& getData() { return config; }
};