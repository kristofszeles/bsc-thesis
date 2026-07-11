#include <algorithm>
#include <cstring>
#include <iostream>

#include "serverthread.h"
#include "server.h"

ServerThread::ServerThread(Server* server, TCPsocket socket, const std::string& id) : server(server), socket(socket), id(id) {
    this->stop = false;
    this->initialized = false;
    this->transportDetected = false;
    this->webSocketClient = false;
}

int ServerThread::receiveBytes(char* out, int maxLength) {
    if (!transportDetected) {
        std::string initialData;
        const int result = webSocket.detect(socket, initialData);
        if (result < 0) return -1;
        transportDetected = true;
        webSocketClient = (result == 1);
        if (!webSocketClient) pendingRaw = initialData;
    }
    if (webSocketClient) {
        return webSocket.recvPayload(socket, out, maxLength);
    }
    // Hand out bytes consumed during transport detection first.
    if (!pendingRaw.empty()) {
        const int count = std::min((int)pendingRaw.size(), maxLength);
        std::memcpy(out, pendingRaw.data(), (size_t)count);
        pendingRaw.erase(0, (size_t)count);
        return count;
    }
    return SDLNet_TCP_Recv(socket, out, maxLength);
}

bool ServerThread::sendBytes(const char* data, int length) {
    // Broadcasts (Server::handleMessageQueue) and this thread's replies can
    // write concurrently; serialize them so WebSocket frames (and TCP message
    // boundaries) never interleave.
    std::lock_guard<std::mutex> lock(sendMutex);
    if (webSocketClient) {
        return webSocket.sendPayload(socket, data, length);
    }
    return SDLNet_TCP_Send(socket, data, length) >= length;
}

void ServerThread::run() {
    while (!stop) {
        IPaddress* remoteip = SDLNet_TCP_GetPeerAddress(socket);
        if (!remoteip) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_TCP_GetPeerAddress", SDLNet_GetError(), nullptr);
            continue;
        }

        if (!init()) break;

        while (!stop) {
            receiveMessages();
            while (!messageQueue.empty()) {
                std::string message = messageQueue.front();
                messageQueue.pop();
                if (DEBUG_MODE) std::cout << "Received message from " << server->getPlayerName(id) << ": " << message << std::endl;
                try {
                    handleMessage(json::parse(message));
                } catch (std::exception& ex) {
                    if (DEBUG_MODE) std::cout << ex.what() << std::endl;
                }
            }
        }
    }
    disconnect();
}

void ServerThread::disconnect() {
    if (!stop) {
        SDLNet_TCP_Close(socket);
        stop = true;
    }
}

bool ServerThread::init() {
    receiveMessages();
    if (messageQueue.empty()) {
        disconnect();
        return false;
    }
    try {
        json message = json::parse(messageQueue.front());
        messageQueue.pop();
        if (server->getCounter() >= server->getConfig()["server"]["maxSlots"]) {
            sendAction("max-slot-error");
            disconnect();
            return false;
        }
        if (server->isPlayerNameExists(message["playerName"])) {
            sendAction("already-exists-error");
            disconnect();
            return false;
        }
        server->addCounter(1);
        server->setPlayerName(id, message["playerName"]);
        server->setPlayerVehicle(id, message["vehicle"]);
        server->setPlayerScore(id, 0);
        server->setPlayerPosition(id, server->getMap()->getStart()->getX(), server->getMap()->getStart()->getZ(), server->getMap()->getStart()->getAngle());
        sendInitGame();
        initialized = true;
        server->broadcastJoinGame(id);
    } catch (std::exception& ex) {
        if (DEBUG_MODE) std::cout << ex.what() << std::endl;
        return false;
    }
    return true;
}

void ServerThread::receiveMessages() {
    char message[MESSAGE_BUFFER_SIZE];
    int length = 0;
    std::string messages;
    bool stop = false;
    do {
        length = receiveBytes(message, MESSAGE_BUFFER_SIZE);
        for (int i = 0; i < length; ++i) {
            messages += message[i];
            if (message[i] == ';') {
                stop = true;
            }
        }
    } while (length > 0 && !stop);
    if (length <= 0) {
        if (server->isPlayerExists(id)) {
            server->broadcastLeaveGame(id);
            server->removePlayer(id);
            server->broadcastHighScores();
            server->addCounter(-1);
        }
        disconnect();
        return;
    }
    std::string buffer;
    for (unsigned int i = 0; i < messages.size(); ++i) {
        char character = messages[i];
        if (character != ';') {
            buffer += character;
        } else if (!buffer.empty()) {
            messageQueue.push(buffer);
            buffer = "";
        }
    }
}

void ServerThread::handleMessage(const json& message) {
    std::string action = message["action"];
    if (action == "move-player") {
        float x1 = message["x"];
        float z1 = message["z"];
        float angle = message["angle"];
        float x2 = server->getPlayer(id)["x"];
        float z2 = server->getPlayer(id)["z"];
        if (abs(x1 - x2) < 2 && abs(z1 - z2) < 2) { // cheat prevention
            server->setPlayerPosition(id, x1, z1, angle);
            server->checkCollision(id);
            server->broadcastMovePlayer(id, x1, z1, angle);
        }
    } else if (action == "chat-message") {
        server->broadcastChatMessage(id, message["text"]);
    }
}

void ServerThread::sendMessage(const json& message) {
    std::string data = message.dump() + ';';
    int length = data.size();
    if (length) {
        if (!sendBytes(data.c_str(), length)) {
            if (DEBUG_MODE) std::cout << "sendBytes: " << SDLNet_GetError() << std::endl;
        }
    }
    if (DEBUG_MODE) std::cout << "Sent message: " << data << std::endl;
}

void ServerThread::sendInitGame() {
    json message;
    message["playerId"] = id;
    message["action"] = "init-game";
    message["players"] = server->getPlayers().dump();
    message["scores"] = server->getScores().dump();
    message["map"] = server->getMap()->getData();
    sendMessage(message);
}

void ServerThread::sendAction(const std::string& action) {
    json message;
    message["action"] = action;
    sendMessage(message);
}