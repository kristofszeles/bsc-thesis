//#define CATCH_CONFIG_MAIN

#ifndef CATCH_CONFIG_MAIN

#include <SDL_main.h>

#include "game.h"

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

int main(int argc, char* argv[]) {
    srand((int)time(nullptr));

#ifdef __APPLE__
    maze_macos_chdir_to_bundle_resources_if_needed();
#endif

    Game game;
    game.run();

    return 0;
}

#else

#include <catch.hpp>
#include <fstream>
#include <sstream>
#include "config.h"
#include "map.h"
#include "entity.h"
#include "camera.h"
#include "editor.h"

TEST_CASE("Initializes and saves configuration correctly", "[Config]") {
    Config config("test/got/game-config.json");
    config.init();
    config.save();
    std::string want, got;
    std::ifstream file;
    std::ostringstream ss;
    file.open("test/want/game-config.json");
    ss << file.rdbuf();
    want = ss.str();
    ss.str("");
    file.close();
    file.open("test/got/game-config.json");
    ss << file.rdbuf();
    got = ss.str();
    file.close();
    CHECK(want == got);
}

TEST_CASE("Map saves state correctly", "[Map::saveState]") {
    Map map;
    map.addEntity(new Tile( {0, 0, 0}, nullptr ));
    map.saveState("test/got/test1.map");
    std::string want, got;
    std::ifstream file;
    std::ostringstream ss;
    file.open("test/want/test1.map");
    ss << file.rdbuf();
    want = ss.str();
    ss.str("");
    file.close();
    file.open("test/got/test1.map");
    ss << file.rdbuf();
    got = ss.str();
    file.close();
    CHECK(want == got);
}

TEST_CASE("Map loads entities correctly", "[Map::loadFile]") {
    Map map;
    map.loadFile("./test/want/test2.map");
    Position position(5, -1.5f, 3, 90);
    Entity* entity = map.getPlayer();
    CHECK(typeid(*entity) == typeid(Player));
    CHECK(entity->getPosition().x == position.x);
    CHECK(entity->getPosition().y == position.y);
    CHECK(entity->getPosition().z == position.z);
    CHECK(entity->getPosition().angleY == position.angleY);
}

TEST_CASE("Two entities collide", "[Entity::checkCollision]") {
    Entity entity1({ 1, 0, 0 }, nullptr);
    Entity entity2({ 0, 0, 0 }, nullptr);
    CHECK(entity1.checkCollision(&entity2) == 4);
}

TEST_CASE("Two entities don't collide", "[Entity::checkCollision]") {
    Entity entity1({ 10, 0, 0 }, nullptr);
    Entity entity2({ 0, 0, 0 }, nullptr);
    CHECK(entity1.checkCollision(&entity2) == 0);
}

TEST_CASE("Camera calculates correct position", "[Camera::getPosition]") {
    Camera camera;
    camera.setYaw(215);
    camera.setPitch(20);
    CHECK(roundf(camera.getPosition().x * 100) / 100 == -0.54f);
    CHECK(roundf(camera.getPosition().y * 100) / 100 == 0.34f);
    CHECK(roundf(camera.getPosition().z * 100) / 100 == -0.77f);
}

TEST_CASE("Editor saves map correctly", "[Editor::saveMap]") {
    Editor editor;
    editor.setSkyboxTexture("skybox2.png");
    editor.setTileTexture("Tile2.png");
    editor.addBlock(new Block("Tile", 7, 10, 0, 0));
    editor.saveMap("test/got/editor.map");
    std::string want, got;
    std::ifstream file;
    std::ostringstream ss;
    file.open("test/want/editor.map");
    ss << file.rdbuf();
    want = ss.str();
    ss.str("");
    file.close();
    file.open("test/got/editor.map");
    ss << file.rdbuf();
    got = ss.str();
    file.close();
    CHECK(want == got);
}

TEST_CASE("Editor loads map correctly", "[Editor::loadMap]") {
    Editor editor;
    editor.loadMap("test/want/editor.map");
    Block* block = new Block("Tile", 7, 10, 0, 0);
    CHECK(editor.getBlocks().front()->type == block->type);
    CHECK(editor.getBlocks().front()->x == block->x);
    CHECK(editor.getBlocks().front()->y == block->y);
    CHECK(editor.getBlocks().front()->z == block->z);
    CHECK(editor.getBlocks().front()->angle == block->angle);
}

TEST_CASE("Editor selects a block", "[Editor::selectBlockAt]") {
    Editor editor;
    Block* block = new Block("Tile", 10, 15, 0, 0);
    editor.addBlock(block);
    editor.selectBlockAt(10, 15);
    CHECK(editor.getSelectedBlock() == block);
}

#endif