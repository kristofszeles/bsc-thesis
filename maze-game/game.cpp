#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <memory>
#include <cmath>

#include <SDL_system.h>
#include <SDL_hints.h>
#include <SDL_touch.h>

#include "game.h"

#if defined(__ANDROID__)
// Prefer event.tfinger.touchId; SDL_GetTouchDevice(0) is not always the on-screen device on Android.
static SDL_TouchID androidResolveMapTouchId(SDL_TouchID touchIdFromEvent) {
    if (touchIdFromEvent > 0) {
        return touchIdFromEvent;
    }
    const int n = SDL_GetNumTouchDevices();
    for (int i = 0; i < n; ++i) {
        const SDL_TouchID tid = SDL_GetTouchDevice(i);
        if (tid == 0) {
            continue;
        }
        if (SDL_GetTouchDeviceType(tid) == SDL_TOUCH_DEVICE_DIRECT) {
            return tid;
        }
    }
    if (n > 0) {
        return SDL_GetTouchDevice(0);
    }
    return 0;
}
#endif

#if defined(__ANDROID__)
static bool keyIsBackOrEscape(const SDL_Keysym& keysym) {
    return keysym.sym == SDLK_ESCAPE || keysym.sym == SDLK_AC_BACK || keysym.scancode == SDL_SCANCODE_AC_BACK;
}
#else
static bool keyIsBackOrEscape(const SDL_Keysym& keysym) {
    return keysym.sym == SDLK_ESCAPE;
}
#endif
#if !defined(__ANDROID__)
#include <nfd.hpp>
#endif
#include "gl_compat.h"
#include "maze.h"
#include "drawutils.h"
#include "editor.h"
#include "shaders.h"

#if defined(__ANDROID__)
#include "android_picker.h"

static void maze_android_load_asset_lines(const std::string& path, std::vector<std::string>& out) {
    // APK assets are not ordinary filesystem paths; std::ifstream fails. SDL_LoadFile uses the same
    // asset resolution as IMG_Load and the rest of the Android port.
    size_t datasize = 0;
    void* raw = SDL_LoadFile(path.c_str(), &datasize);
    if (!raw) {
        SDL_Log("maze_android_load_asset_lines: could not open %s: %s", path.c_str(), SDL_GetError());
        return;
    }
    const char* buf = static_cast<const char*>(raw);
    std::string content(buf, buf + datasize);
    SDL_free(raw);
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            out.push_back(std::move(line));
        }
    }
}
#endif

// On Android, HUD and menu prerendered labels use 2x the legacy size; desktop keeps legacy sizes.
#if defined(__ANDROID__)
static constexpr float kInGameTextScale = 2.0f;
#else
static constexpr float kInGameTextScale = 1.0f;
#endif

// D-pad chevrons and choose-vehicle arrow buttons: same ">" scale for a square cell of this side
// (d-pad uses full pad size s with cell s/3; we pass that cell size here to keep formulas aligned).
static float chevronTextureScaleForCellSide(float boxSide, float tGlyph) {
    const float t = fmaxf(1.0f, tGlyph);
    const float sAsFullPad = 3.0f * boxSide;
    float ts = fminf(4.2f, fmaxf(3.2f, sAsFullPad * 0.0135f)) * kInGameTextScale;
    const float maxTs = 0.88f * boxSide / t;
    if (ts > maxTs) {
        ts = maxTs;
    }
    return ts;
}

Game::Game() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL_Init", SDL_GetError(), nullptr);
        exit(1);
    }
#if defined(__ANDROID__)
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    // Allow every orientation; the rest of the rendering path (Window's startup size query and
    // drawLoadingScreen's per-draw resync) adapts to whatever surface the OS hands us.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait PortraitUpsideDown LandscapeLeft LandscapeRight");
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
#if defined(__ANDROID__)
    android.game = this;
    android.resetTouchKeyGestures();
#endif
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
#if defined(__ANDROID__)
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
#endif
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
    addTexture("background", createTextureFromImage(assetRoot + "textures/bg.png"));
    addTexture("select", createTextureFromImage(assetRoot + "textures/select.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/numbers.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/uppercase.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/lowercase.png"));
    fonts.push_back(loadImage(assetRoot + "fonts/specials.png"));
    labels.push_back(renderText("3D Maze", fonts.data()));
    labels.push_back(renderText("Game Over", fonts.data()));
    labels.push_back(renderText("Kristof Szeles 2021", fonts.data()));
    labels.push_back(renderText("Loading...", fonts.data()));
    labels.push_back(renderText("Connecting...", fonts.data()));
    labels.push_back(renderText("Choose difficulty", fonts.data()));
    labels.push_back(renderText("Choose vehicle", fonts.data()));
    labels.push_back(renderText("Press SPACE to respawn", fonts.data()));
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
    glDeleteProgram(m_programID_1);
    glDeleteProgram(m_programID_2);
}

void Game::loadTextures() {
#if defined(__ANDROID__)
    std::vector<std::string> names;
    maze_android_load_asset_lines(assetRoot + "filelists/textures_root.txt", names);
    for (const auto& rel : names) {
        std::filesystem::path p(rel);
        Texture* texture = createTextureFromImage(assetRoot + rel);
        addTexture(p.filename().string(), texture);
    }
#else
    {
        std::string path = assetRoot + "textures/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Texture* texture = createTextureFromImage(entry.path().string());
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
        Texture* texture = createTextureFromImage(assetRoot + rel);
        addTexture(p.filename().string(), texture);
        skyboxTextures.push_back(p.filename().string());
    }
#else
    {
        std::string path = assetRoot + "textures/skyboxes/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Texture* texture = createTextureFromImage(entry.path().string());
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
        Texture* texture = createTextureFromImage(assetRoot + rel);
        addTexture(p.filename().string(), texture);
        tileTextures.push_back(p.filename().string());
    }
#else
    {
        std::string path = assetRoot + "textures/tiles/";
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(entry)) continue;
            Texture* texture = createTextureFromImage(entry.path().string());
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
#if !defined(__ANDROID__)
                        setRelativeMouseMode(true);
#endif
                        deleteMenu();
                        camera->reset();
                        loadMap(lastMapPath());
                        setPlayerScore((int)config->getData()["game"]["singlePlayer"]["score"]);
                        map->getPlayer()->setHealth((int)config->getData()["game"]["singlePlayer"]["health"]);
                        // .value() defaults for configs written before these keys existed. Yaw/pitch
                        // must be applied after loadMap, which otherwise overwrites them from the
                        // player's stored angle (Map::loadEntities resets pitch to 0).
                        const auto& sp = config->getData()["game"]["singlePlayer"];
                        const int savedMode = sp.value("cameraMode", 0);
                        if (savedMode == 1) camera->setMode(1);
                        camera->setYaw(sp.value("cameraYaw", 0.0f));
                        camera->setPitch(sp.value("cameraPitch", 0.0f));
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
#if !defined(__ANDROID__)
                        setRelativeMouseMode(true);
#endif
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
#if defined(__ANDROID__)
                {
                    std::string picked;
                    if (maze_android::consumePickedMap(picked)) {
                        screen = Screen::GAME;
                        gameMode = GameMode::SINGLE_PLAYER;
                        deleteMenu();
                        camera->reset();
                        std::stringstream ss(picked);
                        loadMap(ss);
                        setDrawModePerspective();
                        // Skip the difficulty/ESCAPE dispatch below: we have already left the menu.
                        event = 0;
                    }
                }
#endif
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
#if !defined(__ANDROID__)
                    setRelativeMouseMode(true);
#endif
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
                    // Only FPS (0) and TPS (1) are valid in-game view modes; mode 2 is the menu's
                    // choose-vehicle preview and would be nonsense to restore.
                    const int mode = camera->getMode();
                    config->getData()["game"]["singlePlayer"]["cameraMode"] = (mode == 1) ? 1 : 0;
                    config->getData()["game"]["singlePlayer"]["cameraYaw"] = camera->getYaw();
                    config->getData()["game"]["singlePlayer"]["cameraPitch"] = camera->getPitch();
                    map->saveState(lastMapPath());
                }
                deleteMap();
                initMenu();
                screen = Screen::MENU;
                subMenu = SubMenu::MAIN;
                gameMode = GameMode::IN_MENU;
                setRelativeMouseMode(false);
#if defined(__ANDROID__)
                android.resetTouchKeyGestures();
#endif
            }
        } else if (screen == Screen::EDITOR) {
            SDL_GL_SetSwapInterval(0); // disable vsync
            Editor editor(drawUtils);
            editor.run();
            SDL_GL_SetSwapInterval(1); // enable vsync
            screen = Screen::MENU;
            subMenu = SubMenu::MAIN;
        }
#if defined(__ANDROID__)
        android.syncTextInputState();
#endif
    }
}

#if defined(__ANDROID__)
void Game::Android::setTextInputRect() {
    int w = game->window->getWidth();
    int h = game->window->getHeight();
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    SDL_Rect r;
    if (game->screen == Screen::GAME && game->chatMode) {
        float dpx, dpy, dps, dpc;
        dpadGetLayout(dpx, dpy, dps, dpc);
        (void)dpx;
        (void)dpc;
        // IME focus: region from above the d-pad (chat sits there on Android)
        const int y0 = (int)(dpy - 12.0f - 52.0f);
        r.x = 0;
        r.y = (y0 > 0) ? y0 : (int)(h * 0.45f);
        r.w = w;
        r.h = h - r.y;
    } else {
        r.x = 0;
        r.y = (int)(h * 0.35f);
        r.w = w;
        r.h = (int)(h * 0.30f);
    }
    SDL_SetTextInputRect(&r);
}

void Game::Android::requestScreenKeyboardOnTap() {
    if (!game->window || !game->window->getWindow()) return;
    const bool needOnScreenKeyboard =
        (game->screen == Screen::MENU
            && (game->subMenu == SubMenu::ENTER_PLAYER_NAME || game->subMenu == SubMenu::ENTER_SERVER_ADDRESS))
        || (game->screen == Screen::GAME && game->chatMode);
    if (!needOnScreenKeyboard) return;
    if (keyboardDismissedByUser) return;
    const bool shown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
        && (SDL_IsScreenKeyboardShown(game->window->getWindow()) == SDL_TRUE);
    if (shown) return;
    // Force a re-show: gesture-nav back can hide the IME without flipping SDL_IsTextInputActive,
    // so a plain SDL_StartTextInput would be a no-op. Stop first to clear any stale state.
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
    SDL_StartTextInput();
    setTextInputRect();
    prevTextInputActive = true;
    prevKeyboardShown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
        && (SDL_IsScreenKeyboardShown(game->window->getWindow()) == SDL_TRUE);
    SDL_RaiseWindow(game->window->getWindow());
}

void Game::Android::syncTextInputState() {
    if (!game->window || !game->window->getWindow()) return;
    const bool needOnScreenKeyboard =
        (game->screen == Screen::MENU
            && (game->subMenu == SubMenu::ENTER_PLAYER_NAME || game->subMenu == SubMenu::ENTER_SERVER_ADDRESS))
        || (game->screen == Screen::GAME && game->chatMode);
    if (needOnScreenKeyboard) {
        const bool active = SDL_IsTextInputActive();
        const bool shown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
            && (SDL_IsScreenKeyboardShown(game->window->getWindow()) == SDL_TRUE);
        // Detect dismissal two ways: (a) SDL flipped IsTextInputActive off — happens when the
        // IME path through onKeyPreIme fires SDL_StopTextInput; (b) the IME visibility flipped
        // off while SDL still thinks text input is active — happens with Android's gesture-nav
        // back, which hides the IME without delivering KEYCODE_BACK to the app.
        if ((prevTextInputActive && !active) || (prevKeyboardShown && !shown)) {
            keyboardDismissedByUser = true;
        }
        if (!active && !keyboardDismissedByUser) {
            SDL_StartTextInput();
            setTextInputRect();
            prevTextInputActive = SDL_IsTextInputActive();
        } else if (active) {
            setTextInputRect();
            prevTextInputActive = true;
        } else {
            prevTextInputActive = false;
        }
        prevKeyboardShown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
            && (SDL_IsScreenKeyboardShown(game->window->getWindow()) == SDL_TRUE);
    } else {
        if (SDL_IsTextInputActive()) {
            SDL_StopTextInput();
        }
        keyboardDismissedByUser = false;
        prevTextInputActive = false;
        prevKeyboardShown = false;
    }
}

void Game::Android::dismissChatAndKeyboard() {
    if (!game->chatMode) {
        return;
    }
    game->chatMode = false;
    syncTextInputState();
}
#endif

Game::MapEvent Game::handleMapEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exitGame();
            break;
        case SDL_MOUSEMOTION: {
#if defined(__ANDROID__)
            if (event.motion.which == SDL_TOUCH_MOUSEID) break;
#endif
            if (relativeMouseMode) {
                float mouseSensitivity = config->getData()["game"]["mouseSensitivity"];
                if (mouseSensitivity < 0.1f) mouseSensitivity = 0.1f;
                else if (mouseSensitivity > 2.0f) mouseSensitivity = 2.0f;
                // Config scale; touch look uses a separate path (see SDL_FINGERMOTION).
                constexpr float kMouseLookGain = 1.75f;
                const float s = mouseSensitivity * kMouseLookGain;
                camera->adjustYaw(-(event.motion.xrel * s));
                camera->adjustPitch(event.motion.yrel * s);
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
#if defined(__ANDROID__)
            // A single touch also emits a synthetic left click. Do not use it to dismiss: either
            // FINGER will dismiss, or it would fire after FINGER opened chat and kill chat immediately.
            // (Same idea as menu ignoring SDL_TOUCH_MOUSEID on MOUSEBUTTONUP for hit testing.)
            if (event.button.button == SDL_BUTTON_LEFT && chatMode) {
                if (event.button.which == SDL_TOUCH_MOUSEID) {
                    break;
                }
                android.dismissChatAndKeyboard();
                break;
            }
            if (event.button.button == SDL_BUTTON_LEFT && !map->isGameOver()
                && android.viewToggleContainsPoint((float)event.button.x, (float)event.button.y)) {
                android.toggleViewFromPointer();
                break;
            }
            if (event.button.button == SDL_BUTTON_LEFT && !map->isGameOver() && gameMode == GameMode::MULTIPLAYER
                && android.chatButtonContainsPoint((float)event.button.x, (float)event.button.y)) {
                android.openChatFromPointer();
                break;
            }
#endif
#if !defined(__ANDROID__)
            if (event.button.button == SDL_BUTTON_RIGHT) {
                relativeMouseMode = 1 - relativeMouseMode;
                setRelativeMouseMode(relativeMouseMode);
            }
#endif
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
                toggleCameraViewF2();
            }
            else if (event.key.keysym.sym == SDLK_F11) window->toggleMaximized();
            else if (keyIsBackOrEscape(event.key.keysym)) {
                if (chatMode) {
#if defined(__ANDROID__)
                    android.dismissChatAndKeyboard();
#else
                    chatMode = false;
#endif
                } else {
                    return MapEvent::MAP_PRESS_ESCAPE;
                }
            }
            else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                gameMapApplyEnterAction();
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
        case SDL_TEXTINPUT: {
            if (!chatMode) break;
            char character = event.text.text[0];
            if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '!' || character == '?' || character == '.' || character == ':' || character == ' ' || character == '_' || character == '-') {
                if (inputText.size() < 24) inputText += character;
            }
            break;
        }
#if defined(__ANDROID__)
        case SDL_FINGERDOWN: {
            const float w = (float)window->getWidth();
            const float h = (float)window->getHeight();
            const float px = event.tfinger.x * w;
            const float py = event.tfinger.y * h;
            if (chatMode) {
                if (gameMode == GameMode::MULTIPLAYER && android.chatButtonContainsPoint(px, py)) {
                    android.openChatFromPointer();
                } else {
                    android.dismissChatAndKeyboard();
                }
                break;
            }
            if (map->isGameOver()) {
                if (px > w * 0.2f && px < w * 0.8f && py > h * 0.2f && py < h * 0.8f) {
                    config->getData()["game"]["singlePlayer"]["highScore"] = getHighScore();
                    setPlayerScore(0);
                    deleteMap();
                    generateMap(mazeWidth, mazeHeight);
                    android.resetTouchKeyGestures();
                }
                break;
            }
            if (android.viewToggleContainsPoint(px, py)) {
                android.toggleViewFromPointer();
                break;
            }
            if (gameMode == GameMode::MULTIPLAYER && android.chatButtonContainsPoint(px, py)) {
                android.openChatFromPointer();
                break;
            }
            if (!map->isGameOver() && android.dpadBoxContainsPoint(px, py)) {
                android.dpad.active = true;
                android.dpad.fingerId = event.tfinger.fingerId;
                android.dpad.dir = android.dpadDirectionAtPoint(px, py);
            } else if (!map->isGameOver()) {
                if (!android.look.touchActive) {
                    android.look.touchActive = true;
                    android.look.fingerId = event.tfinger.fingerId;
                }
            }
            if (!map->isGameOver() && !android.viewToggleContainsPoint(px, py) && !android.chatButtonContainsPoint(px, py)
                && !android.dpadBoxContainsPoint(px, py)) {
                android.enterTap.active = true;
                android.enterTap.fingerId = event.tfinger.fingerId;
                android.enterTap.startX = px;
                android.enterTap.startY = py;
                android.enterTap.startTicks = SDL_GetTicks();
            } else {
                android.enterTap.active = false;
            }
            android.onTwoFingerStateForMap(event.tfinger.touchId);
            break;
        }
        case SDL_FINGERUP:
            if (event.tfinger.fingerId == android.dpad.fingerId) {
                android.dpad.active = false;
                android.dpad.dir = 0;
            }
            if (event.tfinger.fingerId == android.look.fingerId) {
                android.look.touchActive = false;
            }
            if (map->isGameOver()) {
                android.enterTap.active = false;
                break;
            }
            if (android.enterTap.active && event.tfinger.fingerId == android.enterTap.fingerId) {
                const float w = (float)window->getWidth();
                const float wheight = (float)window->getHeight();
                const float uxp = event.tfinger.x * w;
                const float uyp = event.tfinger.y * wheight;
                const float d = std::hypot(uxp - android.enterTap.startX, uyp - android.enterTap.startY);
                if (d <= 32.0f && (float)(SDL_GetTicks() - android.enterTap.startTicks) <= 450.0f) {
                    gameMapApplyEnterAction();
                }
            }
            if (event.tfinger.fingerId == android.enterTap.fingerId) {
                android.enterTap.active = false;
            }
            break;
        case SDL_FINGERMOTION:
            // TPS pinch-zoom first: it must win over one-finger yaw/pitch (uses event touch device id).
            if (android.updatePinchZoom(event.tfinger.touchId)) {
                break;
            }
            if (android.enterTap.active && event.tfinger.fingerId == android.enterTap.fingerId) {
                const float w = (float)window->getWidth();
                const float h = (float)window->getHeight();
                const float mpx = event.tfinger.x * w;
                const float mpy = event.tfinger.y * h;
                if (std::hypot(mpx - android.enterTap.startX, mpy - android.enterTap.startY) > 32.0f) {
                    android.enterTap.active = false;
                }
            }
            if (event.tfinger.fingerId == android.dpad.fingerId && android.dpad.active) {
                const float w = (float)window->getWidth();
                const float h = (float)window->getHeight();
                const float px = event.tfinger.x * w;
                const float py = event.tfinger.y * h;
                android.dpad.dir = android.dpadDirectionAtPoint(px, py, true);
            } else if (android.look.touchActive && event.tfinger.fingerId == android.look.fingerId) {
                float sens = (float)config->getData()["game"]["mouseSensitivity"];
                if (sens < 0.1f) sens = 0.1f;
                else if (sens > 1.0f) sens = 1.0f;
                const float w = (float)window->getWidth();
                const float h = (float)window->getHeight();
                camera->adjustYaw(-(event.tfinger.dx * w * 0.4f * sens));
                camera->adjustPitch(event.tfinger.dy * h * 0.4f * sens);
            }
            break;
#endif
        }
    }
    return MapEvent::NO_EVENT;
}

void Game::handleMapKeyState() {
    if (!map->getPlayer()) return;
    if (!map->isGameOver()) {
#if defined(__ANDROID__)
        if (android.dpad.active) {
            if (android.dpad.dir != 0) {
                map->getPlayer()->setAcceleration(1);
                map->getPlayer()->setDirection(android.dpad.dir);
            } else
                map->getPlayer()->setAcceleration(0);
            return;
        }
#endif
        if (chatMode) {
            map->getPlayer()->setAcceleration(0);
            return;
        }
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

void Game::changeChooseVehicleBy(int delta) {
    if (subMenu != SubMenu::CHOOSE_VEHICLE || vehicles.empty()) return;
    int idx = (int)config->getData()["game"]["vehicle"];
    idx += delta;
    if (idx < 0) idx = 0;
    const int n = (int)vehicles.size();
    if (idx >= n) idx = n - 1;
    config->getData()["game"]["vehicle"] = idx;
}

void Game::menuChooseVehicleGetArrowButtonLayout(bool left, float& outX, float& outY, float& outS) const {
    const float w = (float)window->getWidth();
    const float h = (float)window->getHeight();
    const float m = 24.0f;
    float s = w < h ? w : h;
    s *= 0.14f;
    if (s < 56.0f) s = 56.0f;
    if (s > 100.0f) s = 100.0f;
    outS = s;
    outY = h * 0.5f - s * 0.5f;
    if (left) {
        outX = m;
    } else {
        outX = w - m - s;
    }
}

int Game::menuPointHitsChooseVehicleArrow(float px, float py) const {
    if (subMenu != SubMenu::CHOOSE_VEHICLE) return -1;
    float lx, ly, ls, rx, ry, rs;
    menuChooseVehicleGetArrowButtonLayout(true, lx, ly, ls);
    menuChooseVehicleGetArrowButtonLayout(false, rx, ry, rs);
    if (px >= lx && py >= ly && px <= lx + ls && py <= ly + ls) return 0;
    if (px >= rx && py >= ry && px <= rx + rs && py <= ry + rs) return 1;
    return -1;
}

void Game::drawMenuChooseVehicleArrows() {
    if (subMenu != SubMenu::CHOOSE_VEHICLE) return;
    const SDL_Color plate = { 40, 40, 52 };
    const SDL_Color inner = { 60, 60, 78 };
    for (int i = 0; i < 2; ++i) {
        const bool isLeft = (i == 0);
        float x, y, s;
        menuChooseVehicleGetArrowButtonLayout(isLeft, x, y, s);
        drawUtils->drawRectangle(plate, x - 2.0f, y - 2.0f, s + 4.0f, s + 4.0f);
        drawUtils->drawRectangle(inner, x, y, s, s);
        std::unique_ptr<Texture> tArr(renderSpecialsGlyph(isLeft ? 7 : 6, fonts.data()));
        const float tGlyph = fmaxf(1.0f, fmaxf((float)tArr->getWidth(), (float)tArr->getHeight()));
        const float scale = chevronTextureScaleForCellSide(s, tGlyph);
        const float tw = tArr->getWidth() * scale;
        const float th = tArr->getHeight() * scale;
        drawUtils->drawTexture2D(tArr.get(), x + (s - tw) * 0.5f, y + (s - th) * 0.5f, scale, 0, 0, 0.0f);
    }
}

int Game::handleMenuEvents() {
    int clickedButtonIndex = 0;
#if defined(__ANDROID__)
    bool menuBlankTapEnter = false;
#endif
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exitGame();
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
#if defined(__ANDROID__)
                if (event.button.which == SDL_TOUCH_MOUSEID) {
                    break;
                }
#endif
                const int hit = menuApplyPointerUpAt((float)event.button.x, (float)event.button.y);
                if (hit > 0) {
                    clickedButtonIndex = hit;
                }
            }
            break;
        case SDL_KEYDOWN:
            if (keyIsBackOrEscape(event.key.keysym)) return MenuEvent::MENU_PRESS_ESCAPE;
            else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) return MenuEvent::MENU_PRESS_ENTER;
            else if (event.key.keysym.sym == SDLK_F11) window->toggleMaximized();
            else if (event.key.keysym.sym == SDLK_LEFT) {
                if (subMenu == SubMenu::CHOOSE_VEHICLE) changeChooseVehicleBy(-1);
            } else if (event.key.keysym.sym == SDLK_RIGHT) {
                if (subMenu == SubMenu::CHOOSE_VEHICLE) changeChooseVehicleBy(1);
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
        case SDL_TEXTINPUT: {
            char character = event.text.text[0];
            if ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '!' || character == '?' || character == '.' || character == ':' || character == ' ' || character == '_' || character == '-') {
                if (inputText.size() < 24) inputText += character;
            }
            break;
        }
#if defined(__ANDROID__)
        case SDL_FINGERDOWN:
            {
                const float w = (float)window->getWidth();
                const float h = (float)window->getHeight();
                const float px = event.tfinger.x * w;
                const float py = event.tfinger.y * h;
                // Android reserves a strip on the left and right edges for the system back gesture.
                // A swipe that starts there often arrives as a short FINGERDOWN/FINGERUP pair and
                // would otherwise be misread as a blank-area tap-to-confirm (ENTER) or a tap-to-
                // raise-keyboard. Skipping the tap tracker for edge touches keeps the back gesture
                // from re-showing the IME the user just dismissed.
                const float edgeInset = fmaxf(48.0f, w * 0.05f);
                if (px <= edgeInset || px >= w - edgeInset) {
                    android.menuEnterTap.active = false;
                } else {
                    android.menuEnterTap.active = true;
                    android.menuEnterTap.fingerId = event.tfinger.fingerId;
                    android.menuEnterTap.x = px;
                    android.menuEnterTap.y = py;
                    android.menuEnterTap.startTicks = SDL_GetTicks();
                }
            }
            break;
        case SDL_FINGERUP: {
            if (event.tfinger.fingerId == android.menuEnterTap.fingerId) {
                const float w = (float)window->getWidth();
                const float h = (float)window->getHeight();
                const float uxp = event.tfinger.x * w;
                const float uyp = event.tfinger.y * h;
                if (android.menuEnterTap.active) {
                    const float edgeInset = fmaxf(48.0f, w * 0.05f);
                    const bool liftedAtEdge = (uxp <= edgeInset || uxp >= w - edgeInset);
                    const float d = std::hypot(uxp - android.menuEnterTap.x, uyp - android.menuEnterTap.y);
                    if (!liftedAtEdge && d <= 32.0f && (float)(SDL_GetTicks() - android.menuEnterTap.startTicks) <= 450.0f) {
                        const bool inInputSubMenu = (subMenu == SubMenu::ENTER_PLAYER_NAME || subMenu == SubMenu::ENTER_SERVER_ADDRESS);
                        const bool imeShown = (SDL_HasScreenKeyboardSupport() == SDL_TRUE)
                            && (SDL_IsScreenKeyboardShown(window->getWindow()) == SDL_TRUE);
                        if (inInputSubMenu && !imeShown) {
                            // Confirmed tap in an input submenu while the IME is dismissed — the
                            // user wants to type again. Clear the dismissal latch and bring it up
                            // instead of advancing the menu via blank-tap-enter.
                            android.keyboardDismissedByUser = false;
                            android.requestScreenKeyboardOnTap();
                        } else {
                            const int hit = menuApplyPointerUpAt(uxp, uyp);
                            if (hit > 0) {
                                clickedButtonIndex = hit;
                            } else if (hit == 0) {
                                menuBlankTapEnter = true;
                            }
                        }
                    }
                }
                android.menuEnterTap.active = false;
            }
            break;
        }
        case SDL_FINGERMOTION:
            if (android.menuEnterTap.active && event.tfinger.fingerId == android.menuEnterTap.fingerId) {
                const float w = (float)window->getWidth();
                const float h = (float)window->getHeight();
                const float mpx = event.tfinger.x * w;
                const float mpy = event.tfinger.y * h;
                if (std::hypot(mpx - android.menuEnterTap.x, mpy - android.menuEnterTap.y) > 32.0f) {
                    android.menuEnterTap.active = false;
                }
            }
            break;
#endif
        }
    }
    if (clickedButtonIndex > 0) {
        return clickedButtonIndex;
    }
#if defined(__ANDROID__)
    if (menuBlankTapEnter) {
        return MenuEvent::MENU_PRESS_ENTER;
    }
#endif
    return 0;
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
    // Async on Android: launch the SAF picker and return. The menu loop polls
    // maze_android::consumePickedMap() each iteration; once the user picks a file the bytes
    // arrive via the JNI callback and are loaded then.
    maze_android::launchOpenMapPicker();
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
#if defined(__ANDROID__)
    android.menuEnterTap = Android::MenuEnterTap{};
#endif
}

void Game::gameMapApplyEnterAction() {
    if (chatMode && !inputText.empty()) {
        client->sendChatMessage(inputText);
        chatMode = false;
#if defined(__ANDROID__)
        android.syncTextInputState();
#endif
    }
}

// Returns: -1 if a choose-vehicle arrow was handled, 0 if no button, 1+ = main/difficulty button index
int Game::menuApplyPointerUpAt(float px, float py) {
    if (subMenu == SubMenu::CHOOSE_VEHICLE) {
        const int hit = menuPointHitsChooseVehicleArrow(px, py);
        if (hit == 0) {
            changeChooseVehicleBy(-1);
            return -1;
        }
        if (hit == 1) {
            changeChooseVehicleBy(1);
            return -1;
        }
    }
    int buttonIndex = 1;
    std::list<Button*> buttons;
    if (subMenu == SubMenu::MAIN) {
        buttons = menu->getButtonGroup1();
    } else if (subMenu == SubMenu::CHOOSE_DIFFICULTY) {
        buttons = menu->getButtonGroup2();
    }
    for (auto& button : buttons) {
        const float scale = button->getScale();
        const float x = window->getWidth() / 2 - button->getWidth() * button->getScale() / 2;
        const float y = button->getY() + window->getHeight() / 2 - button->getHeight() * scale;
        const float w = button->getWidth() * scale * window->getViewportScaleX();
        const float h = button->getHeight() * scale * window->getViewportScaleY();
        if (px >= x && px < x + w && py >= y && py < y + h) {
            return buttonIndex;
        }
        ++buttonIndex;
    }
    return 0;
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
        drawUtils->drawLabel(labels[4], 6.0f * kInGameTextScale);
    } else if (subMenu == SubMenu::CHOOSE_DIFFICULTY) {
        drawUtils->drawBackground2D(textures["background"]);
        {
            const float ms = 5.0f * kInGameTextScale;
            drawUtils->drawTexture2D(
                labels[5],
                window->getWidth() / 2 - labels[5]->getWidth() * ms / 2.0f,
                window->getHeight() / 2.0f - labels[5]->getHeight() * ms - 100.0f * kInGameTextScale,
                ms);
        }
        drawUtils->drawButtonGroup(menu->getButtonGroup2());
    } else if (subMenu == SubMenu::CHOOSE_VEHICLE) {
        drawUtils->drawBackground2D(textures["background"]);
        {
            const float ms = 5.0f * kInGameTextScale;
            drawUtils->drawTexture2D(
                labels[6],
                window->getWidth() / 2 - labels[6]->getWidth() * ms / 2.0f,
                window->getHeight() / 2.0f - labels[6]->getHeight() * ms - 100.0f * kInGameTextScale,
                ms);
        }
        camera->setMode(2);
        setDrawModePerspective();
        Position pos(0, -1, -2, menu->getVehicleAngle());
        drawMesh(pos, vehicleMeshes.at(vehicles[config->getData()["game"]["vehicle"]]), m_programID_2);
        setDrawModeOrtho();
        drawMenuChooseVehicleArrows();
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
    // Match the shader's culling metric: it fades opacity by Euclidean distance from playerPos,
    // reaching fully transparent at renderDistance. Skip anything past that radius on the CPU
    // so we do not run the vertex + fragment pipeline for entities that would draw zero pixels.
    const float renderDistance = (float)(int)config->getData()["game"]["renderDistance"];
    const float renderDistance2 = renderDistance * renderDistance;
    auto withinRenderRadius = [&](const Position& p) {
        const float dx = p.x - playerPos.x;
        const float dy = p.y - playerPos.y;
        const float dz = p.z - playerPos.z;
        return dx * dx + dy * dy + dz * dz <= renderDistance2;
    };
    for (auto& entity : map->getEntities()) {
        const Position& entityPos = entity->getPosition();
        if (!withinRenderRadius(entityPos)) continue;
        if (!entity->isHidden() && !entity->isItem() && entity != map->getPlayer()) {
            drawMesh(entityPos, entity->getMesh(), m_programID_1);
        }
    }
    for (auto& opponent : map->getOpponents()) {
        Position opponentPos = opponent.second->getPosition();
        if (!withinRenderRadius(opponentPos)) continue;
        opponentPos.angleY += 180;
        drawMesh(opponentPos, opponent.second->getMesh(), m_programID_1);
    }
    for (auto& item : map->getItems()) {
        const Position& itemPos = item->getPosition();
        if (!withinRenderRadius(itemPos)) continue;
        if (!item->isHidden()) {
            drawMesh(itemPos, item->getMesh(), m_programID_1);
        }
    }
    for (auto& opponent : map->getOpponents()) {
        Position pos = opponent.second->getPosition();
        if (!withinRenderRadius(pos)) continue;
        if (!opponent.second->getBillboard()) opponent.second->setBillboard(renderText(opponent.second->getName(), fonts.data()));
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
    glCompatReleaseProgram();
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
    glCompatReleaseProgram();
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
    const float g = kInGameTextScale;
    if (gameMode == GameMode::SINGLE_PLAYER) {
        if (!map->isGameOver()) {
            drawUtils->drawText("Score: " + std::to_string(playerScore), 32, 32, 4.0f * g);
            drawUtils->drawText("Health: " + std::to_string(map->getPlayer()->getHealth()), 32, 32.0f + 48.0f * g, 4.0f * g);
        } else {
            drawUtils->drawLabel(labels[1], 6.0f * g);
            std::unique_ptr<Texture> texture1(renderText("Your score: " + std::to_string(playerScore), fonts.data()));
            std::unique_ptr<Texture> texture2(renderText("Best score: " + std::to_string(getHighScore()), fonts.data()));
            const float s1 = 4.0f * g;
            const float s2 = 3.5f * g;
            const float s3 = 2.5f * g;
            drawUtils->drawTexture2D(texture1.get(), window->getWidth() / 2 - texture1->getWidth() * s1 / 2, window->getHeight() / 2 - texture1->getHeight() * s1 / 2 + 48.0f * g, s1);
            drawUtils->drawTexture2D(texture2.get(), window->getWidth() / 2 - texture2->getWidth() * s2 / 2, window->getHeight() / 2 - texture2->getHeight() * s2 / 2 + 96.0f * g, s2);
            drawUtils->drawTexture2D(labels[7], window->getWidth() / 2 - labels[7]->getWidth() * s3 / 2, window->getHeight() / 2 - labels[7]->getHeight() * s3 / 2 + 192.0f * g, s3);
        }
    } else if (gameMode == GameMode::MULTIPLAYER) {
        drawUtils->drawText("High Scores", 32, 32, 4.0f * g);
        float y = 32.0f + 64.0f * g;
        for (unsigned int i = 0; i < client->getHighScores().size(); ++i) {
            float scale = 3.0f * g;
            std::unique_ptr<Texture> texture(renderText(std::to_string(i + 1) + ". " + client->getHighScores().at(i).first + ": " + std::to_string(client->getHighScores().at(i).second), fonts.data()));
            drawUtils->drawTexture2D(texture.get(), 32, y, scale);
            y += texture->getHeight() * scale + 8.0f * g;
        }
#if defined(__ANDROID__)
        float dpx, dpy, dps, dpcell;
        android.dpadGetLayout(dpx, dpy, dps, dpcell);
        (void)dpx;
        (void)dpcell;
        const float gapAboveDpad = 12.0f * g;
        const float inputLineH = 40.0f * g;
        float yInput = dpy - gapAboveDpad - inputLineH;
        if (chatMode) {
            // The Android IME covers the lower portion of the screen, where the input line
            // normally sits (just above the d-pad). Anchor it near the top third — above the
            // typical IME band — so the user can see what they are typing the whole time chat
            // is open, without the box jumping around as the IME shows/hides. The chat message
            // stack is repositioned with it (it draws upward from yInput).
            const float yInputAboveIme = (float)window->getHeight() * 0.35f - inputLineH;
            yInput = fminf(yInput, yInputAboveIme);
        }
        const float chatPanelTop = fmaxf(100.0f, yInput - 220.0f * g);
        y = yInput - 10.0f * g;
        for (int i = client->getChatMessages().size(); i > 0 && i > (int)client->getChatMessages().size() - 6; --i) {
            unsigned int expiration = client->getChatMessages().at(i - 1).second;
            if (expiration < SDL_GetTicks()) continue;
            float scale = 2.0f * g;
            std::unique_ptr<Texture> texture(renderText(client->getChatMessages().at(i - 1).first, fonts.data()));
            if (y < chatPanelTop + 6.0f * g) break;
            drawUtils->drawTexture2D(texture.get(), 24, y, scale);
            y -= texture->getHeight() * scale + 8.0f * g;
        }
        if (chatMode) {
            drawUtils->drawText("> " + inputText + "_", 24, yInput, 2.0f * g);
        }
#else
        y = window->getHeight() - 64.0f * g;
        for (int i = client->getChatMessages().size(); i > 0 && i > (int)client->getChatMessages().size() - 6; --i) {
            unsigned int expiration = client->getChatMessages().at(i - 1).second;
            if (expiration < SDL_GetTicks()) continue;
            float scale = 2.0f * g;
            std::unique_ptr<Texture> texture(renderText(client->getChatMessages().at(i - 1).first, fonts.data()));
            drawUtils->drawTexture2D(texture.get(), 32, y, scale);
            y -= texture->getHeight() * scale + 8.0f * g;
        }
        if (chatMode) drawUtils->drawText("> " + inputText + "_", 32, window->getHeight() - 32.0f * g, 2.0f * g);
#endif
    }
    if (map->getPlayer()->getPotionExpiration() != 0) {
        float scale = 3.0f * g;
        std::unique_ptr<Texture> texture(renderText(map->getPlayer()->getPotionName() + ": " + std::to_string(map->getPlayer()->getPotionExpiration() - time(nullptr)), fonts.data()));
        drawUtils->drawTexture2D(texture.get(), window->getWidth() - texture->getWidth() * scale - 32.0f, 32, scale);
    }
#if defined(__ANDROID__)
    if (!map->isGameOver()) {
        android.drawDpadOverlays();
        if (gameMode == GameMode::MULTIPLAYER) {
            android.drawChatButton();
        }
        android.drawViewToggle();
    }
#endif
    setDrawModePerspective();
}

void Game::drawLoadingScreen() {
    // The surface may have changed size since SDL_CreateWindow — Android's sensorLandscape
    // rotation, immersive/fullscreen transitions, or a follow-up surfaceChanged from the OS
    // all arrive as SDL_WINDOWEVENT_RESIZED. Pump events so SDL applies them to window->w/h,
    // then resync the viewport and 2D projection so the loading screen fills the real surface
    // instead of stretching against a stale viewport.
    SDL_PumpEvents();
    int curW = 0, curH = 0;
    SDL_GetWindowSize(window->getWindow(), &curW, &curH);
    if (curW > 0 && curH > 0 && (curW != window->getWidth() || curH != window->getHeight())) {
        window->setWindowSize(curW, curH);
        window->setViewportSize(curW, curH);
        window->setProjectionMatrixSize(curW, curH);
    }
    setDrawModeOrtho();
    drawUtils->drawBackground2D(textures["background"]);
    drawUtils->drawLabel(labels[3], 6.0f * kInGameTextScale);
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

void Game::toggleCameraViewF2() {
    if (camera->getMode() == 0) {
        camera->setMode(1);
    } else {
        camera->setMode(0);
    }
}

#if defined(__ANDROID__)
void Game::Android::resetTouchKeyGestures() {
    dpad = Dpad{};
    look = Look{};
    enterTap = EnterTap{};
    menuEnterTap = MenuEnterTap{};
    pinchLastDist = 0.0f;
}

void Game::Android::onTwoFingerStateForMap(SDL_TouchID touchIdFromEvent) {
    if (game->map->isGameOver()) return;
    const SDL_TouchID dev = androidResolveMapTouchId(touchIdFromEvent);
    if (dev == 0) return;
    if (SDL_GetNumTouchFingers(dev) < 2) {
        return;
    }
    const SDL_Finger* f0 = SDL_GetTouchFinger(dev, 0);
    const SDL_Finger* f1 = SDL_GetTouchFinger(dev, 1);
    if (!f0 || !f1) {
        return;
    }
    const float w = (float)game->window->getWidth();
    const float h = (float)game->window->getHeight();
    const float x0 = f0->x * w, y0 = f0->y * h, x1 = f1->x * w, y1 = f1->y * h;
    if (dpadBoxContainsPoint(x0, y0) || dpadBoxContainsPoint(x1, y1)
        || viewToggleContainsPoint(x0, y0) || viewToggleContainsPoint(x1, y1)
        || chatButtonContainsPoint(x0, y0) || chatButtonContainsPoint(x1, y1)) {
        // D-pad, View, or Chat: pinch is not this gesture.
        return;
    }
    // Two-finger interaction in the main play area: cancel look/enter and start pinch baseline in TPS.
    look.touchActive = false;
    enterTap.active = false;
    if (game->camera->getMode() == 1) {
        pinchLastDist = 0.0f;
    }
}

bool Game::Android::updatePinchZoom(SDL_TouchID touchIdFromEvent) {
    if (game->map->isGameOver() || game->camera->getMode() != 1) {
        pinchLastDist = 0.0f;
        return false;
    }
    const SDL_TouchID dev = androidResolveMapTouchId(touchIdFromEvent);
    if (dev == 0) {
        pinchLastDist = 0.0f;
        return false;
    }
    const int nf = (int)SDL_GetNumTouchFingers(dev);
    if (nf < 2) {
        pinchLastDist = 0.0f;
        return false;
    }
    const SDL_Finger* f0 = SDL_GetTouchFinger(dev, 0);
    const SDL_Finger* f1 = SDL_GetTouchFinger(dev, 1);
    if (!f0 || !f1) {
        pinchLastDist = 0.0f;
        return false;
    }
    const float w = (float)game->window->getWidth();
    const float h = (float)game->window->getHeight();
    const float x0 = f0->x * w, y0 = f0->y * h, x1 = f1->x * w, y1 = f1->y * h;
    if (dpadBoxContainsPoint(x0, y0) || dpadBoxContainsPoint(x1, y1)
        || viewToggleContainsPoint(x0, y0) || viewToggleContainsPoint(x1, y1)
        || chatButtonContainsPoint(x0, y0) || chatButtonContainsPoint(x1, y1)) {
        pinchLastDist = 0.0f;
        // Do not claim this motion: d-pad + look can run in parallel; no pinch in HUD mix.
        return false;
    }
    const float dist = std::hypot(x1 - x0, y1 - y0);
    if (pinchLastDist > 0.0f) {
        const float dd = dist - pinchLastDist;
        // Pinch-to-zoom scale (higher = more sensitive; clamped in Camera::addZoomPinch).
        game->camera->addZoomPinch(-0.055f * dd);
    }
    pinchLastDist = dist;
    return true;
}

void Game::Android::viewToggleGetLayout(float& outX, float& outY, float& outS) const {
    const float w = (float)game->window->getWidth();
    const float h = (float)game->window->getHeight();
    const float m = 16.0f;
    float s = w < h ? w : h;
    s *= 0.12f;
    if (s < 56.0f) s = 56.0f;
    if (s > 96.0f) s = 96.0f;
    // Bottom-right; look works on the full screen (except chat/d-pad/View on down), so the button is flush to the margin.
    outS = s;
    outX = w - m - s;
    if (outX < m) {
        outX = m;
    }
    outY = h - m - s;
}

bool Game::Android::viewToggleContainsPoint(float px, float py) const {
    float x, y, s;
    viewToggleGetLayout(x, y, s);
    return px >= x && py >= y && px <= x + s && py <= y + s;
}

void Game::Android::drawViewToggle() {
    float x, y, s;
    viewToggleGetLayout(x, y, s);
    const SDL_Color plate = { 40, 40, 52 };
    const SDL_Color inner = { 60, 60, 78 };
    game->drawUtils->drawRectangle(plate, x - 2.0f, y - 2.0f, s + 4.0f, s + 4.0f);
    game->drawUtils->drawRectangle(inner, x, y, s, s);
    std::unique_ptr<Texture> t(renderText("View", game->fonts.data()));
    float scale = fminf(1.85f, fmaxf(1.15f, s * 0.09f)) * kInGameTextScale;
    {
        const float pad = 4.0f;
        const float maxSc = fminf((s - pad) / fmaxf(1.0f, (float)t->getWidth()), (s - pad) / fmaxf(1.0f, (float)t->getHeight()));
        if (scale > maxSc) {
            scale = maxSc;
        }
    }
    const float tw = t->getWidth() * scale;
    const float th = t->getHeight() * scale;
    game->drawUtils->drawTexture2D(t.get(), x + (s - tw) * 0.5f, y + (s - th) * 0.5f, scale);
}

void Game::Android::toggleViewFromPointer() {
    // A single touch typically generates both SDL_FINGER* and a synthetic mouse event; each would
    // call toggle and cancel out. Coalesce to one flip per user tap.
    static Uint32 s_lastMs = 0;
    const Uint32 t = SDL_GetTicks();
    if (t - s_lastMs < 180) return;
    s_lastMs = t;
    game->toggleCameraViewF2();
}

void Game::Android::chatButtonGetLayout(float& outX, float& outY, float& outS) const {
    if (game->gameMode != GameMode::MULTIPLAYER) {
        outX = outY = outS = 0.0f;
        return;
    }
    float vx, vy, s;
    viewToggleGetLayout(vx, vy, s);
    const float gap = 8.0f;
    outS = s;
    outX = vx - gap - s;
    const float m = 16.0f;
    if (outX < m) {
        outX = m;
    }
    outY = vy;
}

bool Game::Android::chatButtonContainsPoint(float px, float py) const {
    if (game->gameMode != GameMode::MULTIPLAYER) {
        return false;
    }
    float x, y, s;
    chatButtonGetLayout(x, y, s);
    if (s <= 0.0f) {
        return false;
    }
    return px >= x && py >= y && px <= x + s && py <= y + s;
}

void Game::Android::drawChatButton() {
    if (game->gameMode != GameMode::MULTIPLAYER) {
        return;
    }
    float x, y, s;
    chatButtonGetLayout(x, y, s);
    const SDL_Color plate = { 40, 40, 52 };
    const SDL_Color inner = { 60, 60, 78 };
    game->drawUtils->drawRectangle(plate, x - 2.0f, y - 2.0f, s + 4.0f, s + 4.0f);
    game->drawUtils->drawRectangle(inner, x, y, s, s);
    std::unique_ptr<Texture> t(renderText("Chat", game->fonts.data()));
    float scale = fminf(1.85f, fmaxf(1.15f, s * 0.09f)) * kInGameTextScale;
    {
        const float pad = 4.0f;
        const float maxSc = fminf((s - pad) / fmaxf(1.0f, (float)t->getWidth()), (s - pad) / fmaxf(1.0f, (float)t->getHeight()));
        if (scale > maxSc) {
            scale = maxSc;
        }
    }
    const float tw = t->getWidth() * scale;
    const float th = t->getHeight() * scale;
    game->drawUtils->drawTexture2D(t.get(), x + (s - tw) * 0.5f, y + (s - th) * 0.5f, scale);
}

void Game::Android::openChatFromPointer() {
    if (game->map->isGameOver() || game->gameMode != GameMode::MULTIPLAYER) {
        return;
    }
    static Uint32 s_lastMs = 0;
    const Uint32 t = SDL_GetTicks();
    if (t - s_lastMs < 180) {
        return;
    }
    s_lastMs = t;
    if (!game->chatMode) {
        game->chatMode = true;
        game->inputText = "";
        SDL_PumpEvents();
        SDL_FlushEvent(SDL_TEXTINPUT);
    }
    // Explicit user request — defeat any earlier dismissal latch.
    keyboardDismissedByUser = false;
    requestScreenKeyboardOnTap();
}

void Game::Android::dpadGetLayout(float& outX, float& outY, float& outSize, float& outCell) const {
    const float w = (float)game->window->getWidth();
    const float h = (float)game->window->getHeight();
    const float m = 16.0f;
    float s = w < h ? w : h;
    s *= 0.35f;
    if (s < 140.0f) s = 140.0f;
    if (s > 300.0f) s = 300.0f;
    outX = m;
    outY = h - m - s;
    outSize = s;
    outCell = s / 3.0f;
}

bool Game::Android::dpadBoxContainsPoint(float px, float py) const {
    float x0, y0, s, cell;
    dpadGetLayout(x0, y0, s, cell);
    (void)cell;
    return px >= x0 && py >= y0 && px <= x0 + s && py <= y0 + s;
}

// 3x3 grid: four arms (U/D/L/R), center and corners inert = no movement
int Game::Android::dpadDirectionAtPoint(float px, float py, bool allowSlop) const {
    float x0, y0, s, cell;
    dpadGetLayout(x0, y0, s, cell);
    if (px < x0 || px > x0 + s) {
        return 0;
    }
    float pyy = py;
    if (pyy < y0) {
        return 0;
    }
    if (pyy > y0 + s) {
        if (!allowSlop || pyy > y0 + s + 56.0f) {
            return 0;
        }
        pyy = y0 + s;
    }
    int col = (int)((px - x0) / cell);
    if (col > 2) col = 2;
    int row = (int)((pyy - y0) / cell);
    if (row > 2) row = 2;
    if (row == 0 && col == 1) return (int)Direction::FORWARD;
    if (row == 2 && col == 1) return (int)Direction::BACKWARD;
    if (row == 1 && col == 0) return (int)Direction::LEFT;
    if (row == 1 && col == 2) return (int)Direction::RIGHT;
    return 0;
}

void Game::Android::drawDpadOverlays() {
    if (game->map->isGameOver()) return;
    float x0, y0, s, cell;
    dpadGetLayout(x0, y0, s, cell);
    const float oCol[3] = {0.0f, cell, 2.0f * cell};
    const float wCol[3] = {cell, cell, cell};
    const float oRow[3] = {0.0f, cell, 2.0f * cell};
    const float hRow[3] = {cell, cell, cell};
    const SDL_Color plate = { 40, 40, 52 };
    const SDL_Color cellIdle = { 70, 70, 90 };
    const SDL_Color cellHi = { 120, 130, 180 };
    game->drawUtils->drawRectangle(plate, x0 - 4.0f, y0 - 4.0f, s + 8.0f, s + 8.0f);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            int d = 0;
            if (row == 0 && col == 1) d = (int)Direction::FORWARD;
            else if (row == 2 && col == 1) d = (int)Direction::BACKWARD;
            else if (row == 1 && col == 0) d = (int)Direction::LEFT;
            else if (row == 1 && col == 2) d = (int)Direction::RIGHT;
            const bool on = (dpad.dir == d);
            const SDL_Color& c = (d != 0 && on) ? cellHi : (d != 0 ? cellIdle : plate);
            const float gap = 2.0f;
            const float cx = x0 + oCol[col] + gap;
            const float cy = y0 + oRow[row] + gap;
            game->drawUtils->drawRectangle(c, cx, cy, wCol[col] - 2.0f * gap, hRow[row] - 2.0f * gap);
        }
    }
    // Direction glyphs from specials.png: 6=right >, 7=left <, 8=up ^, 9=down (chevron; use renderSpecialsGlyph(9), not letter "v")
    std::unique_ptr<Texture> tR(renderSpecialsGlyph(6, game->fonts.data()));
    std::unique_ptr<Texture> tL(renderSpecialsGlyph(7, game->fonts.data()));
    std::unique_ptr<Texture> tU(renderSpecialsGlyph(8, game->fonts.data()));
    std::unique_ptr<Texture> tD(renderSpecialsGlyph(9, game->fonts.data()));
    const float tGlyph = fmaxf(1.0f, fmaxf(
        fmaxf(fmaxf((float)tR->getWidth(), (float)tR->getHeight()), fmaxf((float)tD->getWidth(), (float)tD->getHeight())),
        fmaxf(fmaxf((float)tL->getWidth(), (float)tL->getHeight()), fmaxf((float)tU->getWidth(), (float)tU->getHeight()))));
    const float ts = chevronTextureScaleForCellSide(cell, tGlyph);
    auto drawArrow = [&](Texture* t, int row, int col) {
        const float aw = t->getWidth() * ts;
        const float ah = t->getHeight() * ts;
        const float ccx = x0 + oCol[col] + 0.5f * wCol[col];
        const float ccy = y0 + oRow[row] + 0.5f * hRow[row];
        game->drawUtils->drawTexture2D(t, ccx - 0.5f * aw, ccy - 0.5f * ah, ts, 0, 0, 0.0f);
    };
    drawArrow(tU.get(), 0, 1);
    drawArrow(tD.get(), 2, 1);
    drawArrow(tL.get(), 1, 0);
    drawArrow(tR.get(), 1, 2);
}

#endif

int Game::getHighScore() const {
    if (playerScore > (int)config->getData()["game"]["singlePlayer"]["highScore"]) return playerScore;
    return config->getData()["game"]["singlePlayer"]["highScore"];
}