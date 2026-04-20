#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>

#include <SDL_system.h>

#include "game.h"
#if !defined(__ANDROID__)
#include <nfd.hpp>
#endif
#include "gl_compat.h"
#include "maze.h"
#include "drawutils.h"
#include "editor.h"
#include "shaders.h"

#if defined(__ANDROID__)
static void maze_android_load_asset_lines(const std::string& path, std::vector<std::string>& out) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) {
            out.push_back(line);
        }
    }
}
#endif

Game::Game() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL_Init", SDL_GetError(), nullptr);
        exit(1);
    }
#if defined(__ANDROID__)
    {
        // SDL_GetBasePath is unsupported on Android; asset paths are relative to the APK asset root.
        assetRoot.clear();
        char* wb = SDL_GetPrefPath("com.bscthesis", "maze-game");
        writableRoot = wb ? std::string(wb) : std::string();
        SDL_free(wb);
    }
#else
    assetRoot = "./";
    writableRoot = "./";
#endif
    if (IMG_Init(IMG_INIT_PNG) == -1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "IMG_Init", IMG_GetError(), nullptr);
        exit(1);
    }
    if (SDLNet_Init() == -1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_Init", SDLNet_GetError(), nullptr);
        exit(1);
    }
    map = nullptr;
    menu = nullptr;
    config = new Config(gameConfigPath());
    window = new Window(WINDOW_TITLE, config->getData()["window"]["defaultWidth"], config->getData()["window"]["defaultHeight"], config->getData()["window"]["maximized"]);
    client = new Client();
    camera = new Camera();
    mazeWidth = mazeHeight = 5;
    playerScore = 0;
    quit = false;
    relativeMouseMode = false;
    chatMode = false;
    screen = Screen::MENU;
    subMenu = SubMenu::MAIN;
    gameMode = GameMode::IN_MENU;
    loadShaders();
    loadResources();
    drawUtils = new DrawUtils(window, fonts.data(), &textures);
    drawLoadingScreen();
    loadTextures();
    loadSkyboxTextures();
    loadTileTextures();
    loadMeshes();
    loadVehicleMeshes();
}

Game::~Game() {
    config->getData()["window"]["defaultWidth"] = window->getWidth();
    config->getData()["window"]["defaultHeight"] = window->getHeight();
    config->getData()["window"]["maximized"] = window->isMaximized();
    delete config;
    client->disconnectFromServer();
    client->join();
    delete client;
    delete window;
    delete camera;
    delete drawUtils;
    deleteMap();
    deleteMenu();
    deleteResources();
    deleteShaders();
    IMG_Quit();
    SDLNet_Quit();
    SDL_Quit();
}

void Game::exitGame() {
    quit = true;
}

void Game::loadResources() {
    addTexture("background", new Texture(createTextureFromImage(assetRoot + "textures/bg.png")));
    addTexture("select", new Texture(createTextureFromImage(assetRoot + "textures/select.png")));
    fonts.push_back(loadImage(assetRoot + "fonts/numbers.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/uppercase.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/lowercase.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/specials.png"));
    labels.push_back(new Texture(renderText("3D Maze", fonts.data())));
    labels.push_back(new Texture(renderText("Game Over", fonts.data())));
    labels.push_back(new Texture(renderText("Kristof Szeles 2021", fonts.data())));
    labels.push_back(new Texture(renderText("Loading...", fonts.data())));
    labels.push_back(new Texture(renderText("Connecting...", fonts.data())));
    labels.push_back(new Texture(renderText("Choose difficulty", fonts.data())));
    labels.push_back(new Texture(renderText("Choose vehicle", fonts.data())));
    labels.push_back(new Texture(renderText("Press SPACE to respawn", fonts.data())));
}

void Game::deleteResources() {
    for (auto& label : labels) delete label;
    for (auto& font : fonts) SDL_FreeSurface(font);
    for (auto& texture : textures) delete texture.second;
    for (auto& mesh : meshes) delete mesh.second;
    for (auto& mesh : vehicleMeshes) delete mesh.second;
    labels.clear();
    fonts.clear();
    textures.clear();
    meshes.clear();
    vehicleMeshes.clear();
}

GLuint Game::loadShader(GLenum shaderType, std::string shaderData) {
    GLuint loadedShader = glCreateShader(shaderType);
    if (loadedShader == 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "glCreateShader", std::to_string(glGetError()).c_str(), nullptr);
        exit(1);
    }
    const char* sourcePointer = shaderData.c_str();
    glShaderSource(loadedShader, 1, &sourcePointer, nullptr);
    glCompileShader(loadedShader);
    GLint result = GL_FALSE;
    int infoLogLength;
    glGetShaderiv(loadedShader, GL_COMPILE_STATUS, &result);
    glGetShaderiv(loadedShader, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (result == GL_FALSE) {
        std::vector<char> infoLog(infoLogLength);
        glGetShaderInfoLog(loadedShader, infoLogLength, nullptr, infoLog.data());
        std::string errorMessage(infoLog.data());
        if (!errorMessage.empty()) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "glCompileShader", errorMessage.c_str(), nullptr);
            exit(1);
        }
    }
    return loadedShader;
}

GLuint Game::linkShaders(const std::string& vertexShader, const std::string& fragmentShader) {
    GLuint m_loc = glCreateProgram();
    GLuint vs_ID = loadShader(GL_VERTEX_SHADER, vertexShader.c_str());
    GLuint fs_ID = loadShader(GL_FRAGMENT_SHADER, fragmentShader.c_str());
    glAttachShader(m_loc, fs_ID);
    glAttachShader(m_loc, vs_ID);
    glBindAttribLocation(m_loc, 0, "vs_in_pos");
    glBindAttribLocation(m_loc, 1, "vs_in_norm");
    glBindAttribLocation(m_loc, 2, "vs_in_tex");
    glLinkProgram(m_loc);
    GLint result = GL_FALSE;
    int infoLogLength = 0;
    glGetProgramiv(m_loc, GL_LINK_STATUS, &result);
    glGetProgramiv(m_loc, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (result == GL_FALSE || infoLogLength != 0) {
        std::vector<char> infoLog(infoLogLength);
        glGetProgramInfoLog(m_loc, infoLogLength, nullptr, infoLog.data());
        std::string errorMessage(infoLog.data());
        if (!errorMessage.empty()) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "glLinkProgram", errorMessage.c_str(), nullptr);
            exit(1);
        }
    }
    glDeleteShader(vs_ID);
    glDeleteShader(fs_ID);
    return m_loc;
}

void Game::loadShaders() {
    m_programID_1 = linkShaders(vertexShader, fragmentShader1);
    m_programID_2 = linkShaders(vertexShaderUnlit, fragmentShader2);
}

void Game::deleteShaders() {
    glDeleteShader(m_programID_1);
    glDeleteShader(m_programID_2);
}

void Game::loadTextures() {
#if defined(__ANDROID__)
    std::vector<std::string> names;
    maze_android_load_asset_lines(assetRoot + "filelists/textures_root.txt", names);
    for (const auto& rel : names) {
        std::filesystem::path p(rel);
        Texture* texture = new Texture(createTextureFromImage(assetRoot + rel));
        addTexture(p.filename().string(), texture);
    }
#else
    {
        std::string path = assetRoot + "textures/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Texture* texture = new Texture(createTextureFromImage(entry.path().string()));
            addTexture(entry.path().filename().string(), texture);
        }
    }
#endif
}

void Game::loadSkyboxTextures() {
#if defined(__ANDROID__)
    std::vector<std::string> names;
    maze_android_load_asset_lines(assetRoot + "filelists/skyboxes.txt", names);
    for (const auto& rel : names) {
        std::filesystem::path p(rel);
        Texture* texture = new Texture(createTextureFromImage(assetRoot + rel));
        addTexture(p.filename().string(), texture);
        skyboxTextures.push_back(p.filename().string());
    }
#else
    {
        std::string path = assetRoot + "textures/skyboxes/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Texture* texture = new Texture(createTextureFromImage(entry.path().string()));
            addTexture(entry.path().filename().string(), texture);
            skyboxTextures.push_back(entry.path().filename().string());
        }
    }
#endif
}

void Game::loadTileTextures() {
#if defined(__ANDROID__)
    std::vector<std::string> names;
    maze_android_load_asset_lines(assetRoot + "filelists/tiles.txt", names);
    for (const auto& rel : names) {
        std::filesystem::path p(rel);
        Texture* texture = new Texture(createTextureFromImage(assetRoot + rel));
        addTexture(p.filename().string(), texture);
        tileTextures.push_back(p.filename().string());
    }
#else
    {
        std::string path = assetRoot + "textures/tiles/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Texture* texture = new Texture(createTextureFromImage(entry.path().string()));
            addTexture(entry.path().filename().string(), texture);
            tileTextures.push_back(entry.path().filename().string());
        }
    }
#endif
}

void Game::loadMeshes() {
#if defined(__ANDROID__)
    std::vector<std::string> names;
    maze_android_load_asset_lines(assetRoot + "filelists/models_root.txt", names);
    for (const auto& rel : names) {
        std::filesystem::path p(rel);
        Mesh* mesh = new Mesh();
        mesh->loadOBJ(assetRoot + rel);
        addMesh(p.filename().string(), mesh);
    }
#else
    {
        std::string path = assetRoot + "models/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Mesh* mesh = new Mesh();
            mesh->loadOBJ(entry.path().string());
            addMesh(entry.path().filename().string(), mesh);
        }
    }
#endif
}

void Game::loadVehicleMeshes() {
#if defined(__ANDROID__)
    std::vector<std::string> names;
    maze_android_load_asset_lines(assetRoot + "filelists/vehicles.txt", names);
    for (const auto& rel : names) {
        std::filesystem::path p(rel);
        Mesh* mesh = new Mesh();
        mesh->loadOBJ(assetRoot + rel);
        addVehicleMesh(p.filename().string(), mesh);
        vehicles.push_back(p.filename().string());
    }
#else
    {
        std::string path = assetRoot + "models/vehicles/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Mesh* mesh = new Mesh();
            mesh->loadOBJ(entry.path().string());
            addVehicleMesh(entry.path().filename().string(), mesh);
            vehicles.push_back(entry.path().filename().string());
        }
    }
#endif
}

void Game::run() {
    initMenu();
    while (!quit) {
        if (screen == Screen::MENU) {
            drawMenu();
            menu->run();
            int event = handleMenuEvents();
            handleCollisions();
            camera->update();
            map->getPlayer()->setAngle(camera->getYaw());
            window->updateMapFrame(view, map->getPlayer()->getPosition(), camera);
            if (subMenu == SubMenu::MAIN) {
                if (event == 1) { // new game
                    gameMode = GameMode::SINGLE_PLAYER;
                    subMenu = SubMenu::CHOOSE_VEHICLE;
                    setPlayerScore(0);
                } else if (event == 2) {  // continue game
                    if (std::filesystem::exists(lastMapPath())) {
                        gameMode = GameMode::SINGLE_PLAYER;
                        screen = Screen::GAME;
                        setRelativeMouseMode(true);
                        deleteMenu();
                        camera->reset();
                        loadMap(lastMapPath());
                        setPlayerScore((int)config->getData()["game"]["singlePlayer"]["score"]);
                        map->getPlayer()->setHealth((int)config->getData()["game"]["singlePlayer"]["health"]);
                        setDrawModePerspective();
                    }
                } else if (event == 3) {  // multiplayer
                    gameMode = GameMode::MULTIPLAYER;
                    subMenu = SubMenu::ENTER_PLAYER_NAME;
                    inputText = (std::string)config->getData()["multiplayer"]["playerName"];
                } else if (event == 4) { // map editor
                    screen = Screen::EDITOR;
                } else if (event == 5 || event == MenuEvent::MENU_PRESS_ESCAPE) {  // quit
                    exitGame();
                }
            } else if (subMenu == SubMenu::ENTER_PLAYER_NAME) {
                if (event == MenuEvent::MENU_PRESS_ESCAPE) {  // go back
                    subMenu = SubMenu::MAIN;
                    gameMode = GameMode::IN_MENU;
                } else if (event == MenuEvent::MENU_PRESS_ENTER) {
                    if (inputText.size() >= 3) {
                        subMenu = SubMenu::CHOOSE_VEHICLE;
                        config->getData()["multiplayer"]["playerName"] = inputText;
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, nullptr, "Player name should be at least 3 characters long", nullptr);
                    }
                }
            } else if (subMenu == SubMenu::ENTER_SERVER_ADDRESS) {
                if (event == MenuEvent::MENU_PRESS_ESCAPE) {  // go back
                    subMenu = SubMenu::CHOOSE_VEHICLE;
                    inputText = (std::string)config->getData()["multiplayer"]["playerName"];
                } else if (event == MenuEvent::MENU_PRESS_ENTER && inputText.size() > 0) { // ok
                    std::string host = "localhost";
                    int port = 9999;
                    if (inputText.find(':') != std::string::npos) {
                        try {
                            host = inputText.substr(0, inputText.find(':'));
                            port = std::stoi(inputText.substr(inputText.find(':') + 1, inputText.size() - 1));
                        }
                        catch (std::exception& ex) {
                            std::cout << ex.what() << std::endl;
                        }
                    } else {
                        // use default port number 9999
                        host = inputText;
                        port = 9999;
                    }
                    config->getData()["multiplayer"]["defaultHost"] = host;
                    config->getData()["multiplayer"]["defaultPort"] = port;
                    subMenu = SubMenu::CONNECTING;
                }
            } else if (subMenu == SubMenu::CONNECTING) {
                std::string host = config->getData()["multiplayer"]["defaultHost"];
                int port = config->getData()["multiplayer"]["defaultPort"];
                client->connectToServer(host.c_str(), port);
                if (client->isConnected()) {
                    client->sendInitPlayer(config->getData()["multiplayer"]["playerName"], vehicles[config->getData()["game"]["vehicle"]]);
                    if (client->init()) {
                        camera->reset();
                        loadReceivedMap();
                        screen = Screen::GAME;
                        gameMode = GameMode::MULTIPLAYER;
                        setRelativeMouseMode(true);
                        deleteMenu();
                        setDrawModePerspective();
                        client->start();
                    } else {
                        client->disconnectFromServer();
                        subMenu = SubMenu::ENTER_SERVER_ADDRESS;
                    }
                } else {
                    subMenu = SubMenu::ENTER_SERVER_ADDRESS;
                }
                SDL_PumpEvents();
                SDL_FlushEvent(SDL_KEYDOWN);
            } else if (subMenu == SubMenu::CHOOSE_DIFFICULTY) {
                std::string difficulty;
                switch (event) {
                case MenuEvent::MENU_PRESS_ESCAPE:
                    subMenu = SubMenu::CHOOSE_VEHICLE;
                    break;
                case 1:
                    difficulty = "easy";
                    break;
                case 2:
                    difficulty = "medium";
                    break;
                case 3:
                    difficulty = "hard";
                    break;
                case 4:
                    openMap();
                    break;
                }
                if (!difficulty.empty()) {
                    mazeWidth = config->getData()["maze"][difficulty]["width"];
                    mazeHeight = config->getData()["maze"][difficulty]["height"];
                    screen = Screen::GAME;
                    gameMode = GameMode::SINGLE_PLAYER;
                    setRelativeMouseMode(true);
                    deleteMenu();
                    camera->reset();
                    generateMap(mazeWidth, mazeHeight);
                    setDrawModePerspective();
                }
            } else if (subMenu == SubMenu::CHOOSE_VEHICLE) {
                if (event == MenuEvent::MENU_PRESS_ESCAPE) {  // go back
                    if (gameMode == GameMode::SINGLE_PLAYER) {
                        subMenu = SubMenu::MAIN;
                        gameMode = GameMode::IN_MENU;
                    } else if (gameMode == GameMode::MULTIPLAYER) {
                        subMenu = SubMenu::ENTER_PLAYER_NAME;
                    }
                } else if (event == MenuEvent::MENU_PRESS_ENTER) {
                    if (gameMode == GameMode::SINGLE_PLAYER) {
                        subMenu = SubMenu::CHOOSE_DIFFICULTY;
                    } else if (gameMode == GameMode::MULTIPLAYER) {
                        subMenu = SubMenu::ENTER_SERVER_ADDRESS;
                        inputText = (std::string)config->getData()["multiplayer"]["defaultHost"] + ":" + std::to_string((int)config->getData()["multiplayer"]["defaultPort"]);
                    }
                }
            }
        } else if (screen == Screen::GAME) {
            if (gameMode == GameMode::MULTIPLAYER && !client->isReceivedMapLoaded()) {
                loadReceivedMap();
            }
            drawMap();
            runEntities();
            MapEvent event = handleMapEvents();
            handleMapKeyState();
            handleCollisions();
            camera->update();
            map->getPlayer()->setAngle(camera->getYaw());
            drawHUD();
            window->updateMapFrame(view, map->getPlayer()->getPosition(), camera);
            if (event == MapEvent::MAP_PRESS_ESCAPE || (gameMode == GameMode::MULTIPLAYER && !client->isConnected())) {  // exit to menu
                setDrawModeOrtho();
                client->disconnectFromServer();
                client->join();
                if (gameMode == GameMode::SINGLE_PLAYER) {
                    config->getData()["game"]["singlePlayer"]["score"] = playerScore;
                    config->getData()["game"]["singlePlayer"]["health"] = map->getPlayer()->getHealth();
                    map->saveState(lastMapPath());
                }
                deleteMap();
                initMenu();
                screen = Screen::MENU;
                subMenu = SubMenu::MAIN;
                gameMode = GameMode::IN_MENU;
                setRelativeMouseMode(false);
            }
        } else if (screen == Screen::EDITOR) {
            SDL_GL_SetSwapInterval(0); // disable vsync
            Editor editor(drawUtils);
            editor.run();
            SDL_GL_SetSwapInterval(1); // enable vsync
            screen = Screen::MENU;
            subMenu = SubMenu::MAIN;
        }
    }
}

Game::MapEvent Game::handleMapEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exitGame();
            break;
        case SDL_MOUSEMOTION: {
            if (relativeMouseMode) {
                float mouseSensitivity = config->getData()["game"]["mouseSensitivity"];
                if (mouseSensitivity < 0.1f) mouseSensitivity = 0.1f;
                else if (mouseSensitivity > 1.0f) mouseSensitivity = 1.0f;
                camera->adjustYaw(-(event.motion.xrel * mouseSensitivity));
                camera->adjustPitch(event.motion.yrel * mouseSensitivity);
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            if (camera->getMode() == 1) {
                if (event.wheel.y < 0) camera->zoomOut();
                else if (event.wheel.y > 0) camera->zoomIn();
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_RIGHT) {
                relativeMouseMode = 1 - relativeMouseMode;
                setRelativeMouseMode(relativeMouseMode);
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_SPACE) {
                if (map->isGameOver()) {
                    config->getData()["game"]["singlePlayer"]["highScore"] = getHighScore();
                    setPlayerScore(0);
                    deleteMap();
                    generateMap(mazeWidth, mazeHeight);
                }
            } else if (event.key.keysym.sym == SDLK_F2) {
                if (camera->getMode() == 0) camera->setMode(1);
                else camera->setMode(0);
            }
            else if (event.key.keysym.sym == SDLK_F11) window->toggleMaximized();
            else if (event.key.keysym.sym == SDLK_ESCAPE) {
                if (chatMode) chatMode = false;
                else return MapEvent::MAP_PRESS_ESCAPE;
            }
            else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                if (chatMode && !inputText.empty()) {
                    client->sendChatMessage(inputText);
                    chatMode = false;
                }
            } else if (event.key.keysym.sym == SDLK_t) {
                if (!chatMode && gameMode == GameMode::MULTIPLAYER) {
                    chatMode = true;
                    inputText = "";
                    SDL_PumpEvents();
                    SDL_FlushEvent(SDL_TEXTINPUT);
                }
            } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (inputText.size() > 0) inputText.pop_back();
            } else if (event.key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL) {
                std::string clipboardText = SDL_GetClipboardText();
                // filter special characters
                for (auto& character : clipboardText) {
                    if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '!' || character == '?' || character == '.' || character == ':' || character == ' ' || character == '_' || character == '-') {
                        if (inputText.size() < 24) inputText += character;
                    }
                }
            } else if (event.key.keysym.sym == SDLK_c && SDL_GetModState() & KMOD_CTRL) {
                SDL_SetClipboardText(inputText.c_str());
            }
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window->setWindowSize(event.window.data1, event.window.data2);
                window->setViewportSize(event.window.data1, event.window.data2);
                window->setProjectionMatrixSize(event.window.data1, event.window.data2);
            }
            break;
        case SDL_TEXTINPUT:
            if (!chatMode) break;
            char character = event.text.text[0];
            if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '!' || character == '?' || character == '.' || character == ':' || character == ' ' || character == '_' || character == '-') {
                if (inputText.size() < 24) inputText += character;
            }
            break;
        }
    }
    return MapEvent::NO_EVENT;
}

void Game::handleMapKeyState() {
    if (!map->getPlayer()) return;
    if (!map->isGameOver() && !chatMode) {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_W]) {
            map->getPlayer()->setAcceleration(1);
            if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) map->getPlayer()->setDirection(Direction::FORWARD_LEFT);
            else if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) map->getPlayer()->setDirection(Direction::FORWARD_RIGHT);
            else if (keyState[SDL_SCANCODE_DOWN] || keyState[SDL_SCANCODE_S]) map->getPlayer()->setAcceleration(0);
            else map->getPlayer()->setDirection(Direction::FORWARD);
        } else if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) {
            map->getPlayer()->setAcceleration(1);
            if (keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_W]) map->getPlayer()->setDirection(Direction::FORWARD_LEFT);
            else if (keyState[SDL_SCANCODE_DOWN] || keyState[SDL_SCANCODE_S]) map->getPlayer()->setDirection(Direction::BACKWARD_LEFT);
            else if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) map->getPlayer()->setAcceleration(0);
            else map->getPlayer()->setDirection(Direction::LEFT);
        } else if (keyState[SDL_SCANCODE_DOWN] || keyState[SDL_SCANCODE_S]) {
            map->getPlayer()->setAcceleration(1);
            if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) map->getPlayer()->setDirection(Direction::BACKWARD_LEFT);
            else if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) map->getPlayer()->setDirection(Direction::BACKWARD_RIGHT);
            else if (keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_W]) map->getPlayer()->setAcceleration(0);
            else map->getPlayer()->setDirection(Direction::BACKWARD);
        } else if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) {
            map->getPlayer()->setAcceleration(1);
            if (keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_W]) map->getPlayer()->setDirection(Direction::FORWARD_RIGHT);
            else if (keyState[SDL_SCANCODE_DOWN] || keyState[SDL_SCANCODE_S]) map->getPlayer()->setDirection(Direction::BACKWARD_RIGHT);
            else if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) map->getPlayer()->setAcceleration(0);
            else map->getPlayer()->setDirection(Direction::RIGHT);
        } else {
            map->getPlayer()->setAcceleration(0);
        }
    } else {
        map->getPlayer()->setAcceleration(0);
    }
}

int Game::handleMenuEvents() {
    int clickedButtonIndex = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exitGame();
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int buttonIndex = 1;
                std::list<Button*> buttons;
                if (subMenu == SubMenu::MAIN) buttons = menu->getButtonGroup1();
                else if (subMenu == SubMenu::CHOOSE_DIFFICULTY) buttons = menu->getButtonGroup2();
                for (auto& button : buttons) {
                    float scale = button->getScale();
                    float x = window->getWidth() / 2 - button->getWidth() * button->getScale() / 2;
                    float y = button->getY() + window->getHeight() / 2 - button->getHeight() * scale;
                    float w = button->getWidth() * scale * window->getViewportScaleX();
                    float h = button->getHeight() * scale * window->getViewportScaleY();
                    if (event.button.x >= x && event.button.x < x + w && event.button.y >= y && event.button.y < y + h) {
                        clickedButtonIndex = buttonIndex;
                        break;
                    }
                    ++buttonIndex;
                }
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) return MenuEvent::MENU_PRESS_ESCAPE;
            else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) return MenuEvent::MENU_PRESS_ENTER;
            else if (event.key.keysym.sym == SDLK_F11) window->toggleMaximized();
            else if (event.key.keysym.sym == SDLK_LEFT) {
                if (subMenu == SubMenu::CHOOSE_VEHICLE && config->getData()["game"]["vehicle"] > 0) config->getData()["game"]["vehicle"] = (int)config->getData()["game"]["vehicle"] - 1;
            }
            else if (event.key.keysym.sym == SDLK_RIGHT) {
                if (subMenu == SubMenu::CHOOSE_VEHICLE && config->getData()["game"]["vehicle"] < (int)vehicles.size() - 1) config->getData()["game"]["vehicle"] = (int)config->getData()["game"]["vehicle"] + 1;
            }
            else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (inputText.size() > 0) inputText.pop_back();
            } else if (event.key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL) {
                std::string clipboardText = SDL_GetClipboardText();
                // filter special characters
                for (auto& character : clipboardText) {
                    if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '!' || character == '?' || character == '.' || character == ':' || character == ' ' || character == '_' || character == '-') {
                        if (inputText.size() < 24) inputText += character;
                    }
                }
            } else if (event.key.keysym.sym == SDLK_c && SDL_GetModState() & KMOD_CTRL) {
                SDL_SetClipboardText(inputText.c_str());
            }
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window->setWindowSize(event.window.data1, event.window.data2);
                window->setViewportSize(event.window.data1, event.window.data2);
                window->setProjectionMatrixSize(event.window.data1, event.window.data2);
            }
            break;
        case SDL_TEXTINPUT:
            char character = event.text.text[0];
            if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '!' || character == '?' || character == '.' || character == ':' || character == ' ' || character == '_' || character == '-') {
                if (inputText.size() < 24) inputText += character;
            }
            break;
        }
    }
    return clickedButtonIndex;
}

void Game::runAutoPlay() {
    Position playerPos;
    if (map->getPlayer()) playerPos = map->getPlayer()->getPosition();
    if (!map->getPlayer()->getMovingDirection() && !map->getSolution().empty()) {
        std::pair<int, int> target = map->getSolution().front();
        map->getSolution().pop();
        float x = (target.first + 1) * 3.0f + (target.first) * 3.0f;
        float z = (target.second + 1) * 3.0f + (target.second) * 3.0f;
        map->getPlayer()->gotoPosition(x, z);
        if (playerPos.x > x) camera->rotateTo(90);
        else if (playerPos.x < x) camera->rotateTo(270);
        else if (playerPos.z > z) camera->rotateTo(0);
        else if (playerPos.z < z) camera->rotateTo(180);
    }
}

void Game::runEntities() {
    Position playerPos;
    if (map->getPlayer()) playerPos = map->getPlayer()->getPosition();
    for (auto& entity : map->getEntities()) {
        Position entityPos = entity->getPosition();
        int renderDistance = config->getData()["game"]["renderDistance"];
        if (entityPos.x >= playerPos.x - renderDistance && entityPos.x <= playerPos.x + renderDistance && entityPos.z >= playerPos.z - renderDistance && entityPos.z <= playerPos.z + renderDistance) {
            entity->run();
        }
    }
    if (client->isReceivedMapLoaded()) client->sendPlayerPosition(map->getPlayer());
}

void Game::handleCollisions() {
    for (auto& entity1 : map->getEntities()) {
        if (typeid(*entity1) == typeid(Player)) {
            for (auto& entity2 : map->getEntities()) {
                if (!(entity2->getPosition().x >= map->getPlayer()->getPosition().x - COLLISION_DISTANCE && entity2->getPosition().x <= map->getPlayer()->getPosition().x + COLLISION_DISTANCE && entity2->getPosition().z >= map->getPlayer()->getPosition().z - COLLISION_DISTANCE && entity2->getPosition().z <= map->getPlayer()->getPosition().z + COLLISION_DISTANCE)) continue;
                if (map->getPlayer()->checkCollision(entity2) != 0) {
                    if (typeid(*entity2) == typeid(Finish) || typeid(*entity2) == typeid(Emerald) || typeid(*entity2) == typeid(Gem) || typeid(*entity2) == typeid(Gold) || typeid(*entity2) == typeid(Ruby) || typeid(*entity2) == typeid(FastPotion) || typeid(*entity2) == typeid(SlowPotion)) {
                        if (gameMode == GameMode::IN_MENU || gameMode == GameMode::SINGLE_PLAYER) {
                            if (!entity2->isHidden()) {
                                entity2->hide();
                                if (typeid(*entity2) == typeid(Emerald)) {
                                    addPlayerScore(500);
                                } else if (typeid(*entity2) == typeid(Gem)) {
                                    addPlayerScore(1000);
                                } else if (typeid(*entity2) == typeid(Gold)) {
                                    addPlayerScore(400);
                                } else if (typeid(*entity2) == typeid(Ruby)) {
                                    addPlayerScore(800);
                                } else if (typeid(*entity2) == typeid(FastPotion)) {
                                    map->getPlayer()->setMaxVelocity(0.4f);
                                    map->getPlayer()->activatePotion("Fast Potion", 5);
                                } else if (typeid(*entity2) == typeid(SlowPotion)) {
                                    map->getPlayer()->setMaxVelocity(0.1f);
                                    map->getPlayer()->activatePotion("Slow Potion", 10);
                                } else if (typeid(*entity2) == typeid(Finish)) {
                                    addPlayerScore(10000);
                                    deleteMap();
                                    generateMap(mazeWidth, mazeHeight);
                                    return;
                                }
                            }
                        }
                    } else if (typeid(*entity2) == typeid(NPC)) {
                        map->getPlayer()->substractHealth(1);
                    } else if (typeid(*entity2) == typeid(Tile)) {
                        map->getPlayer()->resolveCollision(entity2);
                    }
                }
            }
        } else if (typeid(*entity1) == typeid(NPC)) {
            for (auto& entity2 : map->getEntities()) {
                if (entity1->checkCollision(entity2) != 0) {
                    if (typeid(*entity2) == typeid(Tile)) {
                        entity1->resolveCollision(entity2);
                        int direction = rand() % 2 == 0 ? entity1->getDirection() + 1 : entity1->getDirection() - 1;
                        if (direction > 4) direction = 1;
                        else if (direction < 1) direction = 4;
                        entity1->setDirection(direction);
                    }
                }
            }
        }
    }
}

void Game::openMap() {
#if defined(__ANDROID__)
    (void)0;
#else
    NFD::Guard nfdGuard;
    NFD::UniquePath fileName;
    nfdfilteritem_t filterItem[2] = { { "MAP file", "map" } };
    nfdresult_t result = NFD::OpenDialog(fileName, filterItem, 1);
    if (result == NFD_OKAY) {
        screen = Screen::GAME;
        gameMode = GameMode::SINGLE_PLAYER;
        setRelativeMouseMode(true);
        deleteMenu();
        camera->reset();
        loadMap(fileName.get());
        setDrawModePerspective();
    } else if (result == NFD_CANCEL) {
        return;
    } else {
        std::cout << "Error: " << NFD::GetError() << std::endl;
    }
#endif
}

void Game::loadMap(const std::string& fileName) {
    std::ifstream file;
    std::stringstream data;
    file.open(fileName);
    data << file.rdbuf();
    file.close();
    loadMap(data);
}

void Game::loadMap(std::stringstream& data, const std::string& skyboxTexture, const std::string& tileTexture) {
    drawLoadingScreen();
    deleteMap();
    map = new Map(&textures, &meshes, &vehicleMeshes);
    map->loadEntities(data, skyboxTexture, tileTexture, vehicleMeshes.at(vehicles[config->getData()["game"]["vehicle"]]));
    if (map->getPlayer()) {
        camera->setYaw(map->getPlayer()->getAngle());
        camera->setPitch(0);
    }
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_MOUSEMOTION);
}

void Game::loadReceivedMap() {
    deleteMap();
    loadMap(client->getReceivedMap());
    client->setMap(map);
    client->loadOpponents();
    client->setReceivedMapLoaded(true);
}

void Game::generateMap(int width, int height) {
    drawLoadingScreen();
    Maze maze(width, height);
    loadMap(maze.getData(), getRandomSkyboxTexture(), getRandomTileTexture());
    map->loadSolution(maze.getSolutionPath());
}

void Game::deleteMap() {
    client->setReceivedMapLoaded(false);
    if (map) {
        delete map;
        map = nullptr;
    }
}

void Game::initMenu() {
    menu = new Menu(fonts.data());
    generateMap(mazeWidth, mazeHeight);
}

void Game::deleteMenu() {
    if (menu) {
        delete menu;
        menu = nullptr;
    }
}

void Game::drawMenu() {
    if (subMenu == SubMenu::MAIN) {
        camera->setMode(0);
        setDrawModePerspective();
        drawMap();
        runEntities();
        runAutoPlay();
        setDrawModeOrtho();
        drawUtils->drawButtonGroup(menu->getButtonGroup1());
        drawUtils->drawLogo(labels[0]);
        drawUtils->drawAuthor(labels[2]);
    } else if (subMenu == SubMenu::ENTER_PLAYER_NAME) {
        drawUtils->drawBackground2D(textures["background"]);
        drawUtils->drawTextInput("Enter player name: ", inputText);
    } else if (subMenu == SubMenu::ENTER_SERVER_ADDRESS) {
        drawUtils->drawBackground2D(textures["background"]);
        drawUtils->drawTextInput("Enter server address: ", inputText);
    } else if (subMenu == SubMenu::CONNECTING) {
        drawUtils->drawBackground2D(textures["background"]);
        drawUtils->drawLabel(labels[4]);
    } else if (subMenu == SubMenu::CHOOSE_DIFFICULTY) {
        drawUtils->drawBackground2D(textures["background"]);
        drawUtils->drawTexture2D(labels[5], window->getWidth() / 2 - labels[5]->getWidth() * 5 / 2, window->getHeight() / 2 - labels[5]->getHeight() * 5 - 100, 5);
        drawUtils->drawButtonGroup(menu->getButtonGroup2());
    } else if (subMenu == SubMenu::CHOOSE_VEHICLE) {
        drawUtils->drawBackground2D(textures["background"]);
        drawUtils->drawTexture2D(labels[6], window->getWidth() / 2 - labels[6]->getWidth() * 5 / 2, window->getHeight() / 2 - labels[6]->getHeight() * 5 - 100, 5);
        camera->setMode(2);
        setDrawModePerspective();
        Position pos(0, -1, -2, menu->getVehicleAngle());
        drawMesh(pos, vehicleMeshes.at(vehicles[config->getData()["game"]["vehicle"]]), m_programID_2);
        setDrawModeOrtho();
    }
}

void Game::drawMap() {
    drawSkybox();
    drawFloor();
    if (camera->getMode() == 1) {
        Position playerPos = map->getPlayer()->getPosition();
        playerPos.angleY += 180;
        drawMesh(playerPos, vehicleMeshes.at(vehicles[config->getData()["game"]["vehicle"]]), m_programID_1);
    }
    drawEntities();
}

void Game::drawEntities() {
    Position playerPos;
    if (map->getPlayer()) playerPos = map->getPlayer()->getPosition();
    for (auto& entity : map->getEntities()) {
        Position entityPos = entity->getPosition();
        int renderDistance = config->getData()["game"]["renderDistance"];
        if (entityPos.x >= playerPos.x - renderDistance && entityPos.x <= playerPos.x + renderDistance && entityPos.z >= playerPos.z - renderDistance && entityPos.z <= playerPos.z + renderDistance) {
            if (!entity->isHidden() && !entity->isItem() && entity != map->getPlayer()) {
                drawMesh(entity->getPosition(), entity->getMesh(), m_programID_1);
            }
        }
    }
    for (auto& opponent : map->getOpponents()) {
        Position opponentPos = opponent.second->getPosition();
        opponentPos.angleY += 180;
        drawMesh(opponentPos, opponent.second->getMesh(), m_programID_1);
    }
    for (auto& item : map->getItems()) {
        Position itemPos = item->getPosition();
        int renderDistance = config->getData()["game"]["renderDistance"];
        if (itemPos.x >= playerPos.x - renderDistance && itemPos.x <= playerPos.x + renderDistance && itemPos.z >= playerPos.z - renderDistance && itemPos.z <= playerPos.z + renderDistance) {
            if (!item->isHidden()) {
                drawMesh(item->getPosition(), item->getMesh(), m_programID_1);
            }
        }
    }
    for (auto& opponent : map->getOpponents()) {
        if (!opponent.second->getBillboard()) opponent.second->setBillboard(new Texture(renderText(opponent.second->getName(), fonts.data())));
        Position pos = opponent.second->getPosition();
        drawBillboard({ pos.x, pos.y + 2.0f, pos.z }, opponent.second->getBillboard());
    }
}

void Game::drawMesh(const Position& position, Mesh* mesh, GLuint m_loc) {
    if (!mesh) return;
    glUseProgram(m_loc);
    glm::mat4 model = glm::mat4(1.0f) * glm::translate<float>(glm::vec3(position.x, position.y, position.z)) * glm::rotate<float>(glm::radians(position.angleY), glm::vec3(0, 1, 0));
    glm::mat4 modelIT = glm::inverse(glm::transpose(model)); // model inverse transpose
    glm::mat4 mvp = projection * view * model;
    glm::vec3 playerPos(map->getPlayer()->getPosition().x, map->getPlayer()->getPosition().y, map->getPlayer()->getPosition().z);
    glUniformMatrix4fv(glGetUniformLocation(m_loc, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(m_loc, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(m_loc, "modelIT"), 1, GL_FALSE, glm::value_ptr(modelIT));
    glUniform3fv(glGetUniformLocation(m_loc, "playerPos"), 1, glm::value_ptr(playerPos));
    glUniform1i(glGetUniformLocation(m_loc, "renderDistance"), config->getData()["game"]["renderDistance"]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mesh->getTexture());
    glUniform1i(glGetUniformLocation(m_loc, "texImage"), 0);
    glCompatBindVertexArray(mesh->getVaoID());
    glDrawElements(GL_TRIANGLES, mesh->getIndices().size(), GL_UNSIGNED_INT, 0);
    glCompatBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);  // unbind texture
    glUseProgram(0);
}

void Game::drawBillboard(const Position& position, Texture* texture) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_programID_2);
    glm::mat4 model = glm::mat4(1.0f) * glm::translate<float>(glm::vec3(position.x, position.y, position.z));
    glm::mat4 modelIT = glm::inverse(glm::transpose(model)); // model inverse transpose
    glm::mat4 view2;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) view2[i][j] = view[i][j];
    }
    for (int j = 0; j < 4; ++j) view2[3][j] = j == 3 ? 1.0f : 0.0f;
    glm::mat4 mvp = projection * view * model * glm::transpose(view2);
    glUniformMatrix4fv(glGetUniformLocation(m_programID_2, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(m_programID_2, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(m_programID_2, "modelIT"), 1, GL_FALSE, glm::value_ptr(modelIT));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->getData());
    glUniform1i(glGetUniformLocation(m_programID_2, "texImage"), 0);
    Mesh* mesh = meshes.at("billboard.obj");
    glCompatBindVertexArray(mesh->getVaoID());
    glDrawElements(GL_TRIANGLES, mesh->getIndices().size(), GL_UNSIGNED_INT, 0);
    glCompatBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);  // unbind texture
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

void Game::drawSkybox() {
    if (!map->getSkybox()) return;
    Position pos(map->getPlayer()->getPosition().x, map->getPlayer()->getPosition().y + 10, map->getPlayer()->getPosition().z);
    drawMesh(pos, map->getSkybox(), m_programID_2);
}

void Game::drawFloor() {
    if (!map->getFloor()) return;
    Position pos(map->getStartX() - 1.5f, -1.5f, map->getStartZ() - 1.5f);
    drawMesh(pos, map->getFloor(), m_programID_2);
}

void Game::drawHUD() {
    setDrawModeOrtho();
    if (gameMode == GameMode::SINGLE_PLAYER) {
        if (!map->isGameOver()) {
            drawUtils->drawText("Score: " + std::to_string(playerScore), 32, 32, 4.0f);
            drawUtils->drawText("Health: " + std::to_string(map->getPlayer()->getHealth()), 32, 80, 4.0f);
        } else {
            drawUtils->drawLabel(labels[1]);
            Texture texture1(renderText("Your score: " + std::to_string(playerScore), fonts.data()));
            Texture texture2(renderText("Best score: " + std::to_string(getHighScore()), fonts.data()));
            drawUtils->drawTexture2D(&texture1, window->getWidth() / 2 - texture1.getWidth() * 4.0f / 2, window->getHeight() / 2 - texture1.getHeight() * 4.0f / 2 + 48, 4.0f);
            drawUtils->drawTexture2D(&texture2, window->getWidth() / 2 - texture2.getWidth() * 3.5f / 2, window->getHeight() / 2 - texture2.getHeight() * 3.5f / 2 + 96, 3.5f);
            drawUtils->drawTexture2D(labels[7], window->getWidth() / 2 - labels[7]->getWidth() * 2.5f / 2, window->getHeight() / 2 - labels[7]->getHeight() * 2.5f / 2 + 192, 2.5f);
        }
    } else if (gameMode == GameMode::MULTIPLAYER) {
        drawUtils->drawText("High Scores", 32, 32, 4.0f);
        float y = 96;
        for (unsigned int i = 0; i < client->getHighScores().size(); ++i) {
            float scale = 3.0f;
            Texture texture(renderText(std::to_string(i + 1) + ". " + client->getHighScores().at(i).first + ": " + std::to_string(client->getHighScores().at(i).second), fonts.data()));
            drawUtils->drawTexture2D(&texture, 32, y, scale);
            y += texture.getHeight() * scale + 8;
        }
        y = window->getHeight() - 64.0f;
        for (int i = client->getChatMessages().size(); i > 0 && i > (int)client->getChatMessages().size() - 6; --i) {
            unsigned int expiration = client->getChatMessages().at(i - 1).second;
            if (expiration < SDL_GetTicks()) continue;
            float scale = 2.0f;
            Texture texture(renderText(client->getChatMessages().at(i - 1).first, fonts.data()));
            drawUtils->drawTexture2D(&texture, 32, y, scale);
            y -= texture.getHeight() * scale + 8;
        }
        if (chatMode) drawUtils->drawText("> " + inputText + "_", 32, window->getHeight() - 32.0f, 2.0f);
    }
    if (map->getPlayer()->getPotionExpiration() != 0) {
        float scale = 3.0f;
        Texture texture(renderText(map->getPlayer()->getPotionName() + ": " + std::to_string(map->getPlayer()->getPotionExpiration() - time(nullptr)), fonts.data()));
        drawUtils->drawTexture2D(&texture, window->getWidth() - texture.getWidth() * scale - 32.0f, 32, scale);
    }
    setDrawModePerspective();
}

void Game::drawLoadingScreen() {
    setDrawModeOrtho();
    drawUtils->drawBackground2D(textures["background"]);
    drawUtils->drawLabel(labels[3]);
    SDL_GL_SwapWindow(window->getWindow());
    setDrawModePerspective();
}

void Game::setDrawModeOrtho() {
#if defined(__ANDROID__)
    drawUtils->setGLES2DOrtho((float)window->getWidth(), (float)window->getHeight());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
#else
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window->getWidth(), window->getHeight(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

void Game::setDrawModePerspective() {
    float cameraFov = camera->getMode() == 2 ? 45.0f : (float)config->getData()["game"]["cameraFov"];
    projection = glm::perspective(glm::radians(cameraFov), (float)window->getWidth() / window->getHeight(), cameraZNear, cameraZFar);
#if !defined(__ANDROID__)
    glMatrixMode(GL_MODELVIEW);
#endif
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
#if !defined(__ANDROID__)
    glEnable(GL_TEXTURE_2D);
#endif
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Game::setRelativeMouseMode(bool mode) {
    relativeMouseMode = mode;
    if (relativeMouseMode) SDL_SetRelativeMouseMode(SDL_TRUE);
    else SDL_SetRelativeMouseMode(SDL_FALSE);
}

int Game::getHighScore() const {
    if (playerScore > (int)config->getData()["game"]["singlePlayer"]["highScore"]) return playerScore;
    return config->getData()["game"]["singlePlayer"]["highScore"];
}