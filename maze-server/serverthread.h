#pragma once

#include <string>
#include <queue>
#include <SDL_net.h>
#include <nlohmann/json.hpp>
#include <CppThread.h>

using json = nlohmann::json;

class Server;

class ServerThread : public CppThread {
private:
    bool stop, initialized;
    Server* server;
    TCPsocket socket;
    std::string id;
    std::queue<std::string> messageQueue;
public:
    ServerThread(Server* server, TCPsocket socket, const std::string& id);
    virtual ~ServerThread() {}
    void run();
    void disconnect();
    bool init();
    void receiveMessages();
    void handleMessage(const json& message);
    void sendMessage(const json& message);
    void sendInitGame();
    void sendAction(const std::string& action);
    bool isStopped() const { return stop; }
    bool isInitialized() const { return initialized; }
    std::string getPlayerId() const { return id; }
    TCPsocket getSocket() const { return socket; }
};