#include <sstream>
#include <gzip/compress.hpp>

#include "base64.h"
#include "map.h"

Map::Map() {
    skyboxTextures = { "skybox1.png", "skybox2.png", "skybox3.png", "skybox4.png", "skybox5.png", "skybox6.png", "skybox7.png", "skybox8.png" };
    tileTextures = { "Tile1.png", "Tile2.png", "Tile3.png", "Tile4.png", "Tile5.png", "Tile6.png", "Tile7.png", "Tile8.png", "Tile9.png", "Tile10.png", "Tile11.png", "Tile12.png", "Tile13.png", "Tile14.png", "Tile15.png", "Tile16.png", "Tile17.png", "Tile18.png" };
    skyboxTexture = skyboxTextures[rand() % skyboxTextures.size()];
    tileTexture = tileTextures[rand() % tileTextures.size()];
}

Map::~Map() {
    for (auto& entity : entities) {
        delete entity;
    }
}

void Map::loadEntities(std::stringstream& data) {
    if (skyboxTexture.empty()) data >> this->skyboxTexture;
    if (tileTexture.empty()) data >> this->tileTexture;
    std::string type;
    float x, y, z, angle;
    while (data >> type >> x >> y >> z >> angle) {
        if (type == "Tile") entities.push_back(new Tile(x, y, z, angle));
        else if (type == "Item1") entities.push_back(new Gem(x, y, z, angle));
        else if (type == "Item2") entities.push_back(new Emerald(x, y, z, angle));
        else if (type == "Item3") entities.push_back(new Ruby(x, y, z, angle));
        else if (type == "Item4") entities.push_back(new Gold(x, y, z, angle));
        else if (type == "Item5") entities.push_back(new FastPotion(x, y, z, angle));
        else if (type == "Item6") entities.push_back(new SlowPotion(x, y, z, angle));
        else if (type == "Start") {
            Start* entity = new Start(x, y, z, angle);
            start = entity;
            entities.push_back(entity);
        } else if (type == "Finish") {
            Finish* entity = new Finish(x, y, z, angle);
            finish = entity;
            entities.push_back(entity);
        }
    }
}

void Map::removeItemAt(float x, float z) {
    for (auto& item : entities) {
        if (item->getX() == x && item->getZ() == z) {
            removeItem(item);
            break;
        }
    }
}

std::string Map::getData() {
    std::ostringstream ss;
    ss << skyboxTexture << std::endl;
    ss << tileTexture << std::endl;
    for (auto& entity : entities) {
        ss << entity->getType() << " " << entity->getX() << " " << entity->getY() << " " << entity->getZ() << " " << entity->getAngle() << std::endl;
    }
    std::string mazeData = ss.str();
    mazeData = gzip::compress(mazeData.c_str(), mazeData.size(), Z_BEST_COMPRESSION);
    mazeData = base64_encode(mazeData);
    return mazeData;
}