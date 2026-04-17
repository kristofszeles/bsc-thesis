#pragma once

#include <list>
#include <vector>

#include "entity.h"

class Map {
private:
    Start* start;
    Finish* finish;
    std::string skyboxTexture;
    std::string tileTexture;
    std::list<Entity*> entities;
    std::vector<std::string> skyboxTextures;
    std::vector<std::string> tileTextures;
public:
    Map();
    ~Map();
    void loadEntities(std::stringstream& data);
    void setSkyboxTexture(const std::string& value) { this->skyboxTexture = value; }
    void setTileTexture(const std::string& value) { this->tileTexture = value; }
    void removeItem(Entity* item) { entities.erase(std::remove(entities.begin(), entities.end(), item), entities.end()); }
    void removeItemAt(float x, float z);
    Entity* getStart() { return start; }
    Entity* getFinish() { return finish; }
    std::string getData();
    std::list<Entity*>& getEntities() { return entities; }
};