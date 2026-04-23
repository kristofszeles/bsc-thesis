#pragma once

#include <string>
#include <vector>
#include <map>
#include <SDL_net.h>

#include "window.h"
#include "menu.h"
#include "map.h"
#include "client.h"
#include "config.h"

class Game {
private:
    enum class Screen { MENU, GAME, EDITOR };
    enum class SubMenu { MAIN, ENTER_SERVER_ADDRESS, ENTER_PLAYER_NAME, CONNECTING, CHOOSE_DIFFICULTY, CHOOSE_VEHICLE };
    enum class GameMode { IN_MENU, SINGLE_PLAYER, MULTIPLAYER };
    enum class MapEvent { MAP_PRESS_ESCAPE, NO_EVENT };
    enum MenuEvent { MENU_PRESS_ESCAPE = -1, MENU_PRESS_ENTER = -2 };

    const char* WINDOW_TITLE = "Maze Game";
    const int COLLISION_DISTANCE = 5;
    const float cameraZNear = 0.5f, cameraZFar = 500.0f;

    std::string assetRoot;
    std::string writableRoot;
    std::string gameConfigPath() const { return writableRoot + "game-config.json"; }
    std::string lastMapPath() const { return writableRoot + "last.map"; }

    bool quit, relativeMouseMode, chatMode;
    int mazeWidth, mazeHeight;
    int playerScore;
    Window* window;
    Menu* menu;
    Map* map;
    Client* client;
    Camera* camera;
    DrawUtils* drawUtils;
    Config* config;
    Screen screen;
    SubMenu subMenu;
    GameMode gameMode;
    GLuint m_programID_1;
    GLuint m_programID_2;
    glm::mat4 view;
    glm::mat4 projection;
    std::string inputText;
    std::vector<Texture*> labels;
    std::vector<SDL_Surface*> fonts;
    std::vector<std::string> vehicles;
    std::vector<std::string> tileTextures;
    std::vector<std::string> skyboxTextures;
    std::map<std::string, Texture*> textures;
    std::map<std::string, Mesh*> meshes;
    std::map<std::string, Mesh*> vehicleMeshes;

    void exitGame();
    MapEvent handleMapEvents();
    void handleMapKeyState();
    int handleMenuEvents();
    void runAutoPlay();
    void runEntities();
    void handleCollisions();
    void loadResources();
    void deleteResources();
    void loadShaders();
    void deleteShaders();
    void loadTextures();
    void loadMeshes();
    void loadVehicleMeshes();
    void addTexture(const std::string& key, Texture* texture) { textures.insert({ key, texture }); }
    void addMesh(const std::string& key, Mesh* mesh) { meshes.insert({ key, mesh }); }
    void addVehicleMesh(const std::string& key, Mesh* mesh) { vehicleMeshes.insert({ key, mesh }); }
    void initMenu();
    void deleteMenu();
    void loadTileTextures();
    void loadSkyboxTextures();
    void openMap();
    void loadMap(const std::string& fileName);
    void loadMap(std::stringstream& data, const std::string& skyboxTexture = "", const std::string& tileTexture = "");
    void loadReceivedMap();
    void generateMap(int width, int height);
    void deleteMap();
    void drawMenu();
    void drawMap();
    void drawEntities();
    void drawMesh(const Position& position, Mesh* mesh, GLuint m_loc);
    void drawBillboard(const Position& position, Texture* texture);
    void drawSkybox();
    void drawFloor();
    void drawHUD();
    void drawLoadingScreen();
    void setDrawModeOrtho();
    void setDrawModePerspective();
    void setRelativeMouseMode(bool mode);
    void addPlayerScore(int amount) { playerScore += amount; }
    void setPlayerScore(int amount) { playerScore = amount; }
    int getHighScore() const;
    GLuint loadShader(GLenum shaderType, std::string shaderData);
    GLuint linkShaders(const std::string& vertexShader, const std::string& fragmentShader);
    std::string getRandomSkyboxTexture() const {
        if (skyboxTextures.empty()) {
            return "skybox1.png";
        }
        return skyboxTextures[static_cast<size_t>(rand()) % skyboxTextures.size()];
    }
    std::string getRandomTileTexture() const {
        if (tileTextures.empty()) {
            return "Tile1.png";
        }
        return tileTextures[static_cast<size_t>(rand()) % tileTextures.size()];
    }
public:
    Game();
    ~Game();
    void run();
};