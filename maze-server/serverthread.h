#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <queue>
#include <SDL_net.h>
#include <nlohmann/json.hpp>
#include <CppThread.h>

#include "websocket.h"

using json = nlohmann::json;

class Server;

class ServerThread : public CppThread {
private:
    std::atomic<bool> stop, initialized;
    Server* server;
    TCPsocket socket;
    std::string id;
    std::queue<std::string> messageQueue;
    // Transport: native TCP clients and browser WebSocket clients share the
    // same port; the first read decides which one this connection is.
    bool transportDetected;
    bool webSocketClient;
    std::string pendingRaw;  // bytes consumed during detection of a TCP client
    WebSocketTransport webSocket;
    std::mutex sendMutex;    // serializes writes from this thread and broadcasts
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
    // Transport-aware socket I/O (SDLNet_TCP_Recv/Send semantics). sendBytes is
    // also used by Server's broadcast loop instead of writing to the socket
    // directly, so WebSocket clients get correctly framed data.
    int receiveBytes(char* out, int maxLength);
    bool sendBytes(const char* data, int length);
    bool isStopped() const { return stop; }
    bool isInitialized() const { return initialized; }
    std::string getPlayerId() const { return id; }
    TCPsocket getSocket() const { return socket; }
};
