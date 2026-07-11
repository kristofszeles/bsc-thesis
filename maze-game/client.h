#pragma once

#if !defined(__EMSCRIPTEN__)
#include <SDL_net.h>
#else
#include <SDL.h>
#endif
#include <atomic>
#include <string>
#include <sstream>
#include <queue>
#include <CppThread.h>
#include <nlohmann/json.hpp>

#include "entity.h"

using json = nlohmann::json;

const int MESSAGE_BUFFER_SIZE = 10240;
const bool DEBUG_MODE = false;

class Map;

// On the web (Emscripten) the transport is a browser WebSocket instead of a
// raw TCP socket (browsers cannot open raw TCP): no worker thread runs
// (start()/join() are never useful in a browser), and Game::run pumps received
// messages once per frame via pump(). maze-server accepts WebSocket clients
// natively on its normal game port (see maze-server/websocket.h), so the
// browser connects directly to the same host:port as native clients.
class Client : public CppThread {
private:
    std::atomic<bool> connected;
    bool receivedMapLoaded;
    Map* map;
#if !defined(__EMSCRIPTEN__)
    TCPsocket socket;
    SDLNet_SocketSet socketSet;
#endif
    json players, scores;
    float lastPlayerX, lastPlayerZ, lastPlayerAngle;
    std::stringstream receivedMap;
    std::string playerId;
    std::queue<std::string> messageQueue;
    std::vector<std::pair<std::string, int>> highScores;
    std::vector<std::pair<std::string, int>> chatMessages;
public:
    Client();
    virtual ~Client() {
        closeConnection();
#if !defined(__EMSCRIPTEN__)
        SDLNet_FreeSocketSet(socketSet);
#endif
    }
    void run() override;
#if defined(__EMSCRIPTEN__)
    // Single-threaded replacement for the run() worker loop: drain everything
    // the WebSocket has delivered so far and handle it. Called once per frame.
    void pump();
#endif
    void connectToServer(const char* host, Uint16 port);
    void disconnectFromServer();
    void closeConnection();
    void sendMessage(const json& message);
    void receiveMessages(int timeout = 0);
    void clearQueue();
    void setMap(Map* map) { this->map = map; }
    void setPlayerId(const std::string& id) { this->playerId = id; }
    void pushChatMessage(const std::string& message) { chatMessages.push_back(std::pair<std::string, int>(message, SDL_GetTicks() + 5000)); }
    void updateScores(const json& scores);
    void sendInitPlayer(const std::string& playerName, const std::string& vehicle);
    void sendPlayerPosition(Player* player);
    void sendChatMessage(const std::string& text);
    void loadOpponents();
    void handleMessage(const json& message);
    void setReceivedMapLoaded(bool value) { this->receivedMapLoaded = value; }
    bool init();
    bool isConnected() const { return connected; }
    bool isQueueEmpty() const { return messageQueue.empty(); }
    bool isReceivedMapLoaded() const { return receivedMapLoaded; }
    std::string popMessage();
    std::string getPlayerId() const { return playerId; }
    std::vector<std::pair<std::string, int>>& getChatMessages() { return chatMessages; }
    std::vector<std::pair<std::string, int>>& getHighScores() { return highScores; }
    json& getPlayers() { return players; }
    std::stringstream& getReceivedMap() { return receivedMap; }
};