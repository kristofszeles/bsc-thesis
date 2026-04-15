#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <map>

#include "entity.h"
#include "camera.h"
#include "mesh.h"
#include "client.h"

class Map {
private:
    float startX, endX, startZ, endZ;
    Player* player;
    Start* start;
    Finish* finish;
    Mesh* floor;
    Mesh* skybox;
    std::string tileTexture;
    std::string skyboxTexture;
    std::list<Entity*> entities;
    std::list<Item*> items;
    std::queue<std::pair<int, int>> solution;
    std::map<std::string, Opponent*> opponents;
    std::map<std::string, Texture*>* textures;
    std::map<std::string, Mesh*>* meshes;
    std::map<std::string, Mesh*>* vehicleMeshes;

    void generateTileMeshes();
public:
    Map(std::map<std::string, Texture*>* textures = nullptr, std::map<std::string, Mesh*>* meshes = nullptr, std::map<std::string, Mesh*>* vehicleMeshes = nullptr);
    ~Map();
    void loadFile(const std::string& fileName);
    void loadEntities(std::stringstream& data, const std::string& skyboxTexture = "", const std::string& tileTexture = "", Mesh* playerVehicle = nullptr);
    void loadSolution(const std::list<std::pair<int, int>>& data);
    void saveState(const std::string& fileName);
    void addEntity(Entity* entity) { entities.push_back(entity); }
    void addItem(Item* item) { entities.push_back(item); items.push_back(item); }
    void addOpponent(const std::string& key, const Position& pos, const std::string& name, const std::string& vehicle) { opponents.insert({ key, new Opponent(pos, getVehicleMesh(vehicle), name) }); }
    void removeOpponent(const std::string& key) { opponents.erase(key); }
    void setTileTexture(const std::string& fileName) { this->tileTexture = fileName; }
    void setSkyboxTexture(const std::string& fileName) { this->skyboxTexture = fileName; if (textures) skybox->setTexture(textures->at(fileName)->getData()); }
    Player* getPlayer() const { return player; }
    Start* getStart() const { return start; }
    Finish* getFinish() const { return finish; }
    Mesh* getMesh(const std::string& key) const { return meshes ? meshes->at(key) : nullptr; }
    Mesh* getVehicleMesh(const std::string& key) const { return vehicleMeshes ? vehicleMeshes->at(key) : nullptr; }
    Mesh* getRandomVehicleMesh() const;
    Mesh* createFloor(float startX, float startZ, float endX, float endZ);
    Mesh* getFloor() const { return floor; }
    Mesh* getSkybox() const { return skybox; }
    Mesh* createTileMesh(Entity* tile);
    std::list<Entity*>& getEntities() { return entities; }
    std::list<Item*>& getItems() { return items; }
    std::map<std::string, Opponent*>& getOpponents() { return opponents; }
    std::queue<std::pair<int, int>>& getSolution() { return solution; };
    bool isGameOver() const { return player->getHealth() == 0; }
    float getStartX() const { return startX; }
    float getStartZ() const { return startZ; }
    float getEndX() const { return endX; }
    float getEndZ() const { return endZ; }
};