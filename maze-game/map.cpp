#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "map.h"
#include "drawutils.h"

using json = nlohmann::json;

Map::Map(std::map<std::string, Texture*>* textures, std::map<std::string, Mesh*>* meshes, std::map<std::string, Mesh*>* vehicles) : textures(textures), meshes(meshes), vehicleMeshes(vehicles) {
    startX = startZ = endX = endZ = 0;
    player = nullptr;
    start = nullptr;
    finish = nullptr;
    floor = nullptr;
    skybox = getMesh("skybox.obj");
}

Map::~Map() {
    for (auto& entity : entities) {
        delete entity;
        entity = nullptr;
    }
    delete floor;
}

void Map::saveState(const std::string& fileName) {
    std::ofstream output;
    output.open(fileName);
    output << skyboxTexture << std::endl;
    output << tileTexture << std::endl;
    for (auto& entity : entities) {
        if (entity->isHidden()) continue;
        std::string type;
        if (typeid(*entity) == typeid(Player)) type = "Start";
        else type = entity->getType();
        if (typeid(*entity) != typeid(Start)) {
            output << type << " " << (int)std::ceil(entity->getPosition().x) / 3 * 3 << " " << 0 << " " << (int)std::ceil(entity->getPosition().z) / 3 * 3 << " " << entity->getAngle() << std::endl;
        }
    }
    output.close();
}

Mesh* Map::getRandomVehicleMesh() const {
    if (!vehicleMeshes) return nullptr;
    auto it = vehicleMeshes->begin();
    std::advance(it, rand() % vehicleMeshes->size());
    return it->second;
}

void Map::loadFile(const std::string& fileName) {
    std::ifstream file;
    std::stringstream data;
    file.open(fileName);
    data << file.rdbuf();
    file.close();
    loadEntities(data);
}

void Map::loadEntities(std::stringstream& data, const std::string& skyboxTexture, const std::string& tileTexture, Mesh* playerVehicle) {
    if (skyboxTexture.empty()) {
        std::string fileName;
        data >> fileName;
        setSkyboxTexture(fileName);
    } else {
        setSkyboxTexture(skyboxTexture);
    }
    if (tileTexture.empty()) {
        std::string fileName;
        data >> fileName;
        setTileTexture(fileName);
    } else {
        setTileTexture(tileTexture);
    }
    startX = FLT_MAX, startZ = FLT_MAX, endX = FLT_MIN, endZ = FLT_MIN;
    std::string type;
    float x, y, z, angle;
    while (data >> type >> x >> y >> z >> angle) {
        Position pos(x, y, z, angle);
        if (type == "Start") {
            if (!player) {
                player = new Player({ pos.x, -1.5f, pos.z, pos.angleY }, playerVehicle);
                addEntity(player);
                start = new Start(pos, getMesh("start.obj"));
                addItem(start);
            }
        } else if (type == "Finish") {
            finish = new Finish(pos, getMesh("finish.obj"));
            addItem(finish);
        }
        else if (type == "Tile") addEntity(new Tile(pos, nullptr));
        else if (type == "NPC") addEntity(new NPC({ pos.x, -1.5f, pos.z, pos.angleY }, getRandomVehicleMesh()));
        else if (type == "Item1") addItem(new Gem(pos, getMesh("gem.obj")));
        else if (type == "Item2") addItem(new Emerald(pos, getMesh("emerald.obj")));
        else if (type == "Item3") addItem(new Gold(pos, getMesh("gold.obj")));
        else if (type == "Item4") addItem(new Ruby(pos, getMesh("ruby.obj")));
        else if (type == "Item5") addItem(new FastPotion(pos, getMesh("fastpotion.obj")));
        else if (type == "Item6") addItem(new SlowPotion(pos, getMesh("slowpotion.obj")));
        if (pos.x < startX) startX = pos.x;
        if (pos.x > endX) endX = pos.x;
        if (pos.z < startZ) startZ = pos.z;
        if (pos.z > endZ) endZ = pos.z;
    }
    if (meshes) {
        floor = createFloor(startX, startZ, endX, endZ);
        generateTileMeshes();
    }
}

void Map::loadSolution(const std::list<std::pair<int, int>>& data) {
    for (auto& pair : data) {
        solution.push(std::pair<int, int>(pair.first, pair.second));
    }
}

void Map::generateTileMeshes() {
    for (auto& entity1 : entities) {
        if (typeid(*entity1) == typeid(Tile)) {
            for (auto& entity2 : entities) {
                if (typeid(*entity2) == typeid(Tile)) {
                    if (entity1->getPosition().x == entity2->getPosition().x) {
                        if (entity1->getPosition().z == entity2->getPosition().z - 3) {
                            entity1->removeHitBoxSide(2);
                            entity2->removeHitBoxSide(1);
                        }
                    }
                    if (entity1->getPosition().z == entity2->getPosition().z) {
                        if (entity1->getPosition().x == entity2->getPosition().x - 3) {
                            entity1->removeHitBoxSide(4);
                            entity2->removeHitBoxSide(3);
                        }
                    }
                }
            }
        }
    }
    for (auto& entity : entities) {
        if (typeid(*entity) == typeid(Tile)) {
            entity->setMesh(createTileMesh(entity));
        }
    }
}

Mesh* Map::createFloor(float startX, float startZ, float endX, float endZ) {
    Mesh* mesh = new Mesh();
    startX -= 1.5f;
    startZ -= 1.5f;
    endX += 1.5f;
    endZ += 1.5f;
    float width = std::abs(endX - startX);
    float height = std::abs(endZ - startZ);
    mesh->addVertex({ glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec2(0, 0) });
    mesh->addVertex({ glm::vec3(width, 0, 0), glm::vec3(0, 1, 0), glm::vec2(width, 0) });
    mesh->addVertex({ glm::vec3(width, 0, height), glm::vec3(0, 1, 0), glm::vec2(width, height) });
    mesh->addVertex({ glm::vec3(width, 0, height), glm::vec3(0, 1, 0), glm::vec2(width, height) });
    mesh->addVertex({ glm::vec3(0, 0, height), glm::vec3(0, 1, 0), glm::vec2(0, height) });
    mesh->addVertex({ glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec2(0, 0) });
    mesh->addIndex(5);
    mesh->addIndex(4);
    mesh->addIndex(3);
    mesh->addIndex(2);
    mesh->addIndex(1);
    mesh->addIndex(0);
    mesh->setTexture(textures->at(tileTexture)->getData());
    mesh->init();
    return mesh;
}

Mesh* Map::createTileMesh(Entity* tile) {
    Mesh* mesh = new Mesh();
    // top
    mesh->addVertex({ glm::vec3(-1.5, 1.5, 1.5), glm::vec3(0, 1, 0), glm::vec2(0, 0) });
    mesh->addVertex({ glm::vec3(1.5, 1.5, 1.5), glm::vec3(0, 1, 0), glm::vec2(3, 0) });
    mesh->addVertex({ glm::vec3(1.5, 1.5, -1.5), glm::vec3(0, 1, 0), glm::vec2(3, 3) });
    mesh->addVertex({ glm::vec3(-1.5, 1.5, -1.5), glm::vec3(0, 1, 0), glm::vec2(0, 3) });
    mesh->addIndex(0);
    mesh->addIndex(1);
    mesh->addIndex(2);
    mesh->addIndex(2);
    mesh->addIndex(3);
    mesh->addIndex(0);
    int indexOffset = 4;
    if (tile->hasHitBoxSide(1)) {
        // front
        mesh->addVertex({ glm::vec3(-1.5, 1.5, -1.5), glm::vec3(0, 0, 1), glm::vec2(0, 3) });
        mesh->addVertex({ glm::vec3(1.5, 1.5, -1.5), glm::vec3(0, 0, 1), glm::vec2(3, 3) });
        mesh->addVertex({ glm::vec3(1.5, -1.5, -1.5), glm::vec3(0, 0, 1), glm::vec2(3, 0) });
        mesh->addVertex({ glm::vec3(-1.5, -1.5, -1.5), glm::vec3(0, 0, 1), glm::vec2(0, 0) });
        mesh->addIndex(indexOffset + 0);
        mesh->addIndex(indexOffset + 1);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 3);
        mesh->addIndex(indexOffset + 0);
        indexOffset += 4;
    }
    if (tile->hasHitBoxSide(2)) {
        // back
        mesh->addVertex({ glm::vec3(-1.5, -1.5, 1.5), glm::vec3(0, 0, -1), glm::vec2(0, 0) });
        mesh->addVertex({ glm::vec3(1.5, -1.5, 1.5), glm::vec3(0, 0, -1), glm::vec2(3, 0) });
        mesh->addVertex({ glm::vec3(1.5, 1.5, 1.5), glm::vec3(0, 0, -1), glm::vec2(3, 3) });
        mesh->addVertex({ glm::vec3(-1.5, 1.5, 1.5), glm::vec3(0, 0, -1), glm::vec2(0, 3) });
        mesh->addIndex(indexOffset + 0);
        mesh->addIndex(indexOffset + 1);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 3);
        mesh->addIndex(indexOffset + 0);
        indexOffset += 4;
    }
    if (tile->hasHitBoxSide(3)) {
        // left
        mesh->addVertex({ glm::vec3(-1.5, -1.5, -1.5), glm::vec3(-1, 0, 0), glm::vec2(0, 0) });
        mesh->addVertex({ glm::vec3(-1.5, -1.5, 1.5), glm::vec3(-1, 0, 0), glm::vec2(3, 0) });
        mesh->addVertex({ glm::vec3(-1.5, 1.5, 1.5), glm::vec3(-1, 0, 0), glm::vec2(3, 3) });
        mesh->addVertex({ glm::vec3(-1.5, 1.5, -1.5), glm::vec3(-1, 0, 0), glm::vec2(0, 3) });
        mesh->addIndex(indexOffset + 0);
        mesh->addIndex(indexOffset + 1);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 3);
        mesh->addIndex(indexOffset + 0);
        indexOffset += 4;
    }
    if (tile->hasHitBoxSide(4)) {
        // right
        mesh->addVertex({ glm::vec3(1.5, -1.5, 1.5), glm::vec3(1, 0, 0), glm::vec2(0, 0) });
        mesh->addVertex({ glm::vec3(1.5, -1.5, -1.5), glm::vec3(1, 0, 0), glm::vec2(3, 0) });
        mesh->addVertex({ glm::vec3(1.5, 1.5, -1.5), glm::vec3(1, 0, 0), glm::vec2(3, 3) });
        mesh->addVertex({ glm::vec3(1.5, 1.5, 1.5), glm::vec3(1, 0, 0), glm::vec2(0, 3) });
        mesh->addIndex(indexOffset + 0);
        mesh->addIndex(indexOffset + 1);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 2);
        mesh->addIndex(indexOffset + 3);
        mesh->addIndex(indexOffset + 0);
        indexOffset += 4;
    }
    if (typeid(*tile) == typeid(Tile)) {
        mesh->setTexture(textures->at(tileTexture)->getData());
    }
    mesh->setWidth(3);
    mesh->setHeight(3);
    mesh->setDepth(3);
    mesh->init();
    return mesh;
}