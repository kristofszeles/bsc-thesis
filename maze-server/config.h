#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

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
        config["server"]["port"] = 9999;
        config["server"]["maxSlots"] = 5;
        config["server"]["mapFile"] = "";
        config["maze"]["width"] = 10;
        config["maze"]["height"] = 10;
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
    }
    json& getData() { return config; }
};