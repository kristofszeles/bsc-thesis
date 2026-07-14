#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <sole.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "serverthread.h"
#include "server.h"

namespace {

// Returns the local IPv4 the OS would use for outbound traffic — i.e. the
// address a client on the same LAN should connect to. Picks one interface
// instead of dumping every adapter (VPN, docker bridges, virtual NICs).
std::string primaryLocalIPv4String() {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return {};
#else
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return {};
#endif
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

    std::string result;
    if (connect(s, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
        sockaddr_in local{};
#ifdef _WIN32
        int len = sizeof(local);
#else
        socklen_t len = sizeof(local);
#endif
        if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0) {
            char buf[INET_ADDRSTRLEN];
#ifdef _WIN32
            if (InetNtopA(AF_INET, &local.sin_addr, buf, INET_ADDRSTRLEN)) {
#else
            if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
#endif
                result = buf;
            }
        }
    }
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
    return result;
}

}  // namespace

Server::Server() {
    this->stop = false;
    this->counter = 0;
    this->map = nullptr;
    if (SDL_Init(0) == -1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL_Init", SDL_GetError(), nullptr);
        exit(1);
    }
    if (SDLNet_Init() == -1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_Init", SDLNet_GetError(), nullptr);
        exit(1);
    }
    config = new Config(CONFIG_FILE_NAME);
    port = config->getData()["server"]["port"];
    std::cout << "Starting server on port " << port << "..." << std::endl;
    if (SDLNet_ResolveHost(&ip, nullptr, port) == -1) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_ResolveHost", SDLNet_GetError(), nullptr);
        exit(1);
    }
    serverSocket = SDLNet_TCP_Open(&ip);
    if (!serverSocket) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_TCP_Open", SDLNet_GetError(), nullptr);
        exit(1);
    }
    std::cout << "Listening on port " << port;
    std::string addr = primaryLocalIPv4String();
    if (!addr.empty()) {
        std::cout << " (" << addr << ")";
    }
    std::cout << "..." << std::endl;
    std::string mapFile = config->getData()["server"]["mapFile"];
    if (mapFile.empty() || !std::filesystem::exists(mapFile)) generateMap();
    else loadMap(mapFile);
}

Server::~Server() {
    delete config;
    delete map;
    SDLNet_TCP_Close(serverSocket);
    SDLNet_Quit();
    SDL_Quit();
}

void Server::handleMessageQueue() {
    // Take the whole queue under the lock (client threads push into it via
    // broadcastMessage), but send outside it: sendBytes can block, and holding
    // mtx there would stall every client thread.
    std::queue<std::string> pending;
    {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        std::swap(pending, messageQueue);
    }
    while (!pending.empty()) {
        std::string message = pending.front();
        pending.pop();
        message += ';';
        int length = message.size();
        if (!length) continue; // empty string
        if (DEBUG_MODE) std::cout << "Sending " << message << std::endl;
        for (auto& serverThread : serverThreads) {
            if (!serverThread->isStopped() && serverThread->isInitialized()) {
                // Route through the thread's transport: WebSocket clients need
                // the payload framed, and sendBytes serializes concurrent writes.
                if (!serverThread->sendBytes(message.c_str(), length)) {
                    if (DEBUG_MODE) std::cout << "sendBytes: " << SDLNet_GetError() << std::endl;
                }
            }
        }
    }
}

void Server::acceptConnection() {
    TCPsocket clientSocket = SDLNet_TCP_Accept(serverSocket);
    if (!clientSocket) return;
    ServerThread* serverThread = new ServerThread(this, clientSocket, sole::uuid4().str());
    {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        serverThreads.push_back(serverThread);
    }
    serverThread->start();
}

void Server::reapStoppedThreads() {
    std::list<ServerThread*> stopped;
    {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        for (auto it = serverThreads.begin(); it != serverThreads.end();) {
            if ((*it)->isStopped()) {
                stopped.push_back(*it);
                it = serverThreads.erase(it);
            } else {
                ++it;
            }
        }
    }
    // Join outside the lock: a dying thread may still be inside a broadcast*
    // call that needs mtx, and joining it while holding mtx would deadlock.
    for (ServerThread* thread : stopped) {
        thread->join();
        delete thread;
    }
}

void Server::run() {
    while (!stop) {
        handleMessageQueue();
        acceptConnection();
        reapStoppedThreads();
    }
}

void Server::loadMap(const std::string& fileName) {
    std::cout << "Loading map from " << fileName << "..." << std::endl;
    std::ifstream file;
    std::stringstream data;
    file.open(fileName);
    data << file.rdbuf();
    file.close();
    if (map) delete map;
    map = new Map();
    map->loadEntities(data);
}

void Server::generateMap() {
    std::cout << "Generating new maze... ";
    Maze maze(config->getData()["maze"]["width"], config->getData()["maze"]["height"]);
    if (map) delete map;
    map = new Map();
    map->loadEntities(maze.getData());
    std::cout << "Ready" << std::endl;
}

void Server::handleCommand(const std::string& line) {
    // Runs on the main (stdin) thread; serverThreads, players and scores are
    // owned by the server/client threads.
    std::lock_guard<std::recursive_mutex> lck(mtx);
    std::istringstream ss(line);
    std::vector<std::string> params;
    std::string param;
    while (ss >> param) params.push_back(param);
    if (params.empty()) return;
    std::string command = params[0];
    if (command == "stop") {
        stop = true;
    } else if (command == "kickall") {
        broadcastKickAll();
    } else if (command == "kick") {
        if (params.size() < 2) return;
        for (unsigned int i = 1; i < params.size(); ++i) {
            std::string name = params[i];
            bool found = false;
            for (auto& serverThread : serverThreads) {
                if (players[serverThread->getPlayerId()]["name"] == name) {
                    serverThread->sendAction("kick");
                    found = true;
                    break;
                }
            }
            if (!found) std::cout << name << " was not found on the server" << std::endl;
        }
    } else if (command == "generate") {
        broadcastNewMap();
    } else if (command == "players") {
        for (auto& player : players) {
            std::cout << (std::string)player["name"] << std::endl;
        }
    } else if (command == "scores") {
        for (auto& score : scores) {
            std::cout << (std::string)score["name"] << ": " << score["score"] << std::endl;
        }
    } else if (command == "help") {
        std::cout << "List of commands" << std::endl;
        std::cout << "generate - Generate new maze and start new game" << std::endl;
        std::cout << "players - List the names of all connected players" << std::endl;
        std::cout << "scores - List the scores of all connected players" << std::endl;
        std::cout << "kickall - Kick all players from the game" << std::endl;
        std::cout << "kick <playername> - Kick the player with the specified name" << std::endl;
        std::cout << "stop - Stop the server" << std::endl;
    } else {
        std::cout << "Unknown command: " << command << std::endl;
    }
}

void Server::checkCollision(const std::string& id) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    if (!map || !players.contains(id)) {
        return;
    }
    float x = players[id]["x"];
    float z = players[id]["z"];
    // Snapshot entities: iterating while broadcastWinGame/generateMap replaces the map or
    // broadcastPickUpItem/removeItemAt mutates the entity list invalidates range-for iterators.
    std::vector<Entity*> entities(map->getEntities().begin(), map->getEntities().end());
    for (Entity* entity : entities) {
        if (typeid(*entity) == typeid(Tile)) continue;
        if (x + 1.0f >= entity->getX() - 1.0f && x - 1.0f <= entity->getX() + 1.0f && z + 1.0f >= entity->getZ() - 1.0f && z - 1.0f <= entity->getZ() + 1.0f) {
            if (typeid(*entity) == typeid(Finish)) {
                addPlayerScore(id, 10000);
                broadcastHighScores();
                broadcastWinGame(id);
                return;
            } else if (typeid(*entity) == typeid(Emerald) || typeid(*entity) == typeid(Gem) || typeid(*entity) == typeid(Ruby) || typeid(*entity) == typeid(Gold) || typeid(*entity) == typeid(FastPotion) || typeid(*entity) == typeid(SlowPotion)) {
                if (typeid(*entity) == typeid(Emerald)) addPlayerScore(id, 500);
                else if (typeid(*entity) == typeid(Gem)) addPlayerScore(id, 1000);
                else if (typeid(*entity) == typeid(Gold)) addPlayerScore(id, 400);
                else if (typeid(*entity) == typeid(Ruby)) addPlayerScore(id, 800);
                broadcastHighScores();
                broadcastPickUpItem(id, entity->getX(), entity->getZ());
            }
        }
    }
}

void Server::setPlayerName(const std::string& id, const std::string& name) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    players[id]["name"] = name;
}

void Server::setPlayerVehicle(const std::string& id, const std::string& vehicle) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    players[id]["vehicle"] = vehicle;
}

void Server::setPlayerPosition(const std::string& id, float x, float z, float angle) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    players[id]["id"] = id;
    players[id]["x"] = x;
    players[id]["z"] = z;
    players[id]["angle"] = angle;
}

void Server::setPlayerScore(const std::string& id, int value) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    scores[id]["name"] = players[id]["name"];
    scores[id]["score"] = value;
}

void Server::broadcastMessage(const json& message) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    messageQueue.push(message.dump());
}

void Server::broadcastNewMap() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    json message;
    generateMap();
    for (auto& player : players) setPlayerPosition(player["id"], map->getStart()->getX(), map->getStart()->getZ(), map->getStart()->getAngle());
    message["action"] = "new-map";
    message["players"] = players;
    message["map"] = map->getData();
    broadcastMessage(message);
}

void Server::broadcastKickAll() {
    json message;
    message["action"] = "kick";
    broadcastMessage(message);
}

void Server::broadcastJoinGame(const std::string& id) {
    std::cout << getPlayerName(id) << " has connected to the server" << std::endl;
    json message;
    message["action"] = "join-game";
    message["id"] = id;
    message["name"] = getPlayerName(id);
    message["vehicle"] = getPlayer(id)["vehicle"];
    message["x"] = getPlayer(id)["x"];
    message["z"] = getPlayer(id)["z"];
    message["angle"] = getPlayer(id)["angle"];
    broadcastMessage(message);
    broadcastHighScores();
}

void Server::broadcastLeaveGame(const std::string& id) {
    std::cout << getPlayerName(id) << " has disconnected from the server" << std::endl;
    json message;
    message["action"] = "leave-game";
    message["id"] = id;
    message["name"] = getPlayerName(id);
    broadcastMessage(message);
}

void Server::broadcastMovePlayer(const std::string& id, float x, float z, float angle) {
    json message;
    message["action"] = "move-player";
    message["id"] = id;
    message["x"] = x;
    message["z"] = z;
    message["angle"] = angle;
    broadcastMessage(message);
}

void Server::broadcastPickUpItem(const std::string& id, float x, float z) {
    std::cout << getPlayerName(id) << " has picked up an item" << std::endl;
    json message;
    message["action"] = "pick-up-item";
    message["id"] = id;
    message["name"] = getPlayerName(id);
    message["x"] = x;
    message["z"] = z;
    map->removeItemAt(x, z);
    broadcastMessage(message);
}

void Server::broadcastWinGame(const std::string& id) {
    std::cout << getPlayerName(id) << " has won the game" << std::endl;
    json message;
    message["action"] = "win-game";
    message["id"] = id;
    message["name"] = getPlayerName(id);
    broadcastMessage(message);
    broadcastNewMap();
}

void Server::broadcastChatMessage(const std::string& id, const std::string& messageText) {
    std::cout << getPlayerName(id) << ": " << messageText << std::endl;
    json message;
    message["action"] = "chat-message";
    message["id"] = id;
    message["name"] = getPlayerName(id);
    message["text"] = messageText;
    broadcastMessage(message);
}

void Server::broadcastHighScores() {
    json message;
    message["action"] = "high-scores";
    message["scores"] = scores;
    broadcastMessage(message);
}

bool Server::isPlayerNameExists(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    for (auto& player : players) {
        if (player["name"] == name) {
            return true;
        }
    }
    return false;
}