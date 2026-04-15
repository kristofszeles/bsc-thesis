#pragma once

#include <list>
#include <queue>
#include <string>
#include <sstream>
#include <fstream>
#include <SDL.h>
#include <SDL_net.h>
#include <nlohmann/json.hpp>
#include <gzip/compress.hpp>
#include <mutex>
#include <CppThread.h>

#include "maze.h"
#include "map.h"
#include "config.h"

using json = nlohmann::json;

const int MESSAGE_BUFFER_SIZE = 10240;
const bool DEBUG_MODE = false;

class ServerThread;

class Server : public CppThread {
private:
	bool stop;
    int counter;
    int port;
    json players, scores;
    Config* config;
	IPaddress ip;
	TCPsocket serverSocket;
    Map* map;
    std::mutex mtx;
	std::list<ServerThread*> serverThreads;
    std::queue<std::string> messageQueue;
    const std::string CONFIG_FILE_NAME = "server-config.json";

    void run() override;
    void loadMap(const std::string& fileName);
    void generateMap();
    void handleMessageQueue();
    void acceptConnection();
    void broadcastMessage(const json& message);
    void broadcastNewMap();
    void broadcastKickAll();
    void broadcastPickUpItem(const std::string& id, float x, float z);
    void broadcastWinGame(const std::string& id);
public:
    Server();
    ~Server();
    void addCounter(int amount) { this->counter += amount; }
    void handleCommand(const std::string& line);
    void checkCollision(const std::string& id);
    void setPlayerName(const std::string& id, const std::string& name);
    void setPlayerVehicle(const std::string& id, const std::string& vehicle);
    void setPlayerPosition(const std::string& id, float x, float z, float angle);
    void setPlayerScore(const std::string& id, int value);
    void addPlayerScore(const std::string& id, int value) { scores[id]["score"] = (int)scores[id]["score"] + value; }
    void removePlayer(const std::string& id) { players.erase(id); scores.erase(id); }
    void broadcastChatMessage(const std::string& id, const std::string& messageText);
    void broadcastHighScores();
    void broadcastJoinGame(const std::string& id);
    void broadcastLeaveGame(const std::string& id);
    void broadcastMovePlayer(const std::string& id, float x, float z, float angle);
    bool isPlayerExists(const std::string& id) const { return players.contains(id); }
    bool isPlayerNameExists(const std::string& name) const;
    json& getPlayers() { return players; }
    json& getScores() { return scores; }
    json& getConfig() { return config->getData(); }
    json& getPlayer(const std::string& id) { return players[id]; }
    Map* getMap() const { return map; }
    int getCounter() const { return counter; }
    std::string getPlayerName(const std::string& id) const { return players[id]["name"]; }
};