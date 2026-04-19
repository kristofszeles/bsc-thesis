#include <iostream>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include <sole.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "serverthread.h"
#include "server.h"

namespace {

std::vector<std::string> localNetworkIPv4Strings() {
    std::set<std::string> unique;
#ifdef _WIN32
    ULONG bufLen = 15000;
    std::vector<BYTE> buffer(bufLen);
    auto* addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                     nullptr, addresses, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufLen);
        addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                   nullptr, addresses, &bufLen);
    }
    if (ret != NO_ERROR) {
        return {};
    }
    for (auto* aa = addresses; aa; aa = aa->Next) {
        if (aa->OperStatus != IfOperStatusUp) {
            continue;
        }
        for (auto* ua = aa->FirstUnicastAddress; ua; ua = ua->Next) {
            if (!ua->Address.lpSockaddr || ua->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            if (sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                continue;
            }
            char buf[INET_ADDRSTRLEN];
            if (InetNtopA(AF_INET, &sin->sin_addr, buf, INET_ADDRSTRLEN)) {
                unique.insert(buf);
            }
        }
    }
#else
    ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        return {};
    }
    for (ifaddrs* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        auto* a = reinterpret_cast<sockaddr_in*>(p->ifa_addr);
        if (a->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
            continue;
        }
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &a->sin_addr, buf, sizeof(buf))) {
            unique.insert(buf);
        }
    }
    freeifaddrs(ifa);
#endif
    return std::vector<std::string>(unique.begin(), unique.end());
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
    std::vector<std::string> addrs = localNetworkIPv4Strings();
    if (!addrs.empty()) {
        std::cout << " (";
        for (size_t i = 0; i < addrs.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << addrs[i];
        }
        std::cout << ")";
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
    while (!messageQueue.empty()) {
        std::string message = messageQueue.front();
        messageQueue.pop();
        message += ';';
        int length = message.size();
        if (!length) continue; // empty string
        if (DEBUG_MODE) std::cout << "Sending " << message << std::endl;
        for (auto& serverThread : serverThreads) {
            if (!serverThread->isStopped() && serverThread->isInitialized()) {
                if (SDLNet_TCP_Send(serverThread->getSocket(), message.c_str(), length) < length) {
                    if (DEBUG_MODE) std::cout << "SDLNet_TCP_Send: " << SDLNet_GetError() << std::endl;
                }
            }
        }
    }
}

void Server::acceptConnection() {
    TCPsocket clientSocket = SDLNet_TCP_Accept(serverSocket);
    if (!clientSocket) return;
    ServerThread* serverThread = new ServerThread(this, clientSocket, sole::uuid4().str());
    serverThreads.push_back(serverThread);
    serverThread->start();
}

void Server::run() {
    while (!stop) {
        handleMessageQueue();
        acceptConnection();
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
    for (auto& entity : map->getEntities()) {
        if (typeid(*entity) == typeid(Tile)) continue;
        float x = players[id]["x"], z = players[id]["z"];
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
    std::lock_guard<std::mutex> lck(mtx);
    players[id]["name"] = name;
}

void Server::setPlayerVehicle(const std::string& id, const std::string& vehicle) {
    std::lock_guard<std::mutex> lck(mtx);
    players[id]["vehicle"] = vehicle;
}

void Server::setPlayerPosition(const std::string& id, float x, float z, float angle) {
    std::lock_guard<std::mutex> lck(mtx);
    players[id]["id"] = id;
    players[id]["x"] = x;
    players[id]["z"] = z;
    players[id]["angle"] = angle;
}

void Server::setPlayerScore(const std::string& id, int value) {
    std::lock_guard<std::mutex> lck(mtx);
    scores[id]["name"] = players[id]["name"];
    scores[id]["score"] = value;
}

void Server::broadcastMessage(const json& message) {
    std::lock_guard<std::mutex> lck(mtx);
    messageQueue.push(message.dump());
}

void Server::broadcastNewMap() {
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
    for (auto& player : players) {
        if (player["name"] == name) {
            return true;
        }
    }
    return false;
}