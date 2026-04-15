#include <iostream>
#include <gzip/decompress.hpp>

#include "base64.h"
#include "client.h"
#include "map.h"

Client::Client() {
    connected = false;
    receivedMapLoaded = false;
    socket = nullptr;
    map = nullptr;
    socketSet = SDLNet_AllocSocketSet(1);
    lastPlayerX = lastPlayerZ = lastPlayerAngle = 0;
}

void Client::run() {
    while (connected) {
        receiveMessages(100);
        while (!isQueueEmpty()) {
            std::string message = popMessage();
            if (DEBUG_MODE) std::cout << "Received message: " << message << std::endl;
            try {
                handleMessage(json::parse(message));
            }
            catch (std::exception& ex) {
                if (DEBUG_MODE) std::cout << ex.what() << ": '" << message << std::endl;
            }
        }
    }
}

void Client::connectToServer(const char* host, Uint16 port) {
    IPaddress ip;
    if (DEBUG_MODE) std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
    if (SDLNet_ResolveHost(&ip, host, port) == -1) {
        if (DEBUG_MODE) std::cout << "SDLNet_ResolveHost: " << SDLNet_GetError() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_ResolveHost", SDLNet_GetError(), nullptr);
        return;
    }
    socket = SDLNet_TCP_Open(&ip);
    if (!socket) {
        if (DEBUG_MODE) std::cout << "SDLNet_TCP_Open: " << SDLNet_GetError() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDLNet_TCP_Open", SDLNet_GetError(), nullptr);
        return;
    }
    SDLNet_TCP_AddSocket(socketSet, socket);
    if (DEBUG_MODE) std::cout << "Successfully connected to " << host << ":" << port << "." << std::endl;
    clearQueue();
    highScores.clear();
    chatMessages.clear();
    connected = true;
}

void Client::disconnectFromServer() {
    if (connected) {
        if (DEBUG_MODE) std::cout << "Disconnecting from the server..." << std::endl;
        connected = false;
        SDLNet_TCP_DelSocket(socketSet, socket);
        SDLNet_TCP_Close(socket);
    }
}

void Client::sendMessage(const json& message) {
    if (!connected) return;
    std::string data = message.dump() + ';';
    int length = data.size();
    if (SDLNet_TCP_Send(socket, data.c_str(), length) < length) {
        if (DEBUG_MODE) std::cout << "SDLNet_TCP_Send: " << SDLNet_GetError() << std::endl;
    }
    if (DEBUG_MODE) std::cout << "Sent message: " << data << std::endl;
}

void Client::receiveMessages(int timeout) {
    char message[MESSAGE_BUFFER_SIZE];
    int length = 0;
    std::string messages;
    bool received = false;
    if (!timeout) {
        bool stop = false;
        do {
            received = true;
            length = SDLNet_TCP_Recv(socket, message, MESSAGE_BUFFER_SIZE);
            for (int i = 0; i < length; ++i) {
                messages += message[i];
                if (message[i] == ';') {
                    stop = true;
                }
            }
        } while (length > 0 && !stop);
    } else {
        bool stop = false;
        do {
            int numReady = SDLNet_CheckSockets(socketSet, timeout);
            if (numReady > 0) {
                if (SDLNet_SocketReady(socket)) {
                    received = true;
                    length = SDLNet_TCP_Recv(socket, message, MESSAGE_BUFFER_SIZE);
                    for (int i = 0; i < length; ++i) {
                        messages += message[i];
                        if (message[i] == ';') {
                            stop = true;
                        }
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        } while (length > 0 && !stop);
    }
    if (connected && received) {
        if (length <= 0) {
            disconnectFromServer();
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, nullptr, "Server has stopped", nullptr);
            return;
        }
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

bool Client::init() {
    receiveMessages(1000);
    if (!isQueueEmpty()) {
        std::string msg = popMessage();
        try {
            json message = json::parse(msg);
            if (message["action"] == "init-game") {
                std::string decodedData = base64_decode(message["map"]);
                std::string decompressedData = gzip::decompress(decodedData.c_str(), decodedData.size());
                receivedMap.clear();
                receivedMap << decompressedData;
                players = json::parse((std::string)message["players"]);
                updateScores(json::parse((std::string)message["scores"]));
                setPlayerId(message["playerId"]);
                return true;
            } else if (message["action"] == "max-slot-error") {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, nullptr, "The server has exceeded its number of maximum slots", nullptr);
            } else if (message["action"] == "already-exists-error") {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, nullptr, "Player name already exists on the server", nullptr);
            }
        }
        catch (std::exception& ex) {
            if (DEBUG_MODE) std::cout << ex.what() << ": '" << msg << "'" << std::endl;
        }
    }
    // failed to initialize multiplayer
    return false;
}

void Client::loadOpponents() {
    for (auto& player : players) {
        if (!player.contains("id") || !player.contains("name") || !player.contains("x") || !player.contains("z") || !player.contains("angle")) continue;
        if (player["id"] == playerId) continue;
        Position pos(player["x"], -1.5f, player["z"], player["angle"]);
        map->addOpponent(player["id"], pos, player["name"], player["vehicle"]);
    }
}

void Client::handleMessage(const json& message) {
    std::string action = message["action"];
    if (receivedMapLoaded) {
        if (message.contains("id")) {
            std::string id = message["id"];
            if (id != getPlayerId()) {
                if (!map->getOpponents().contains(id)) {
                    if (action == "join-game") {
                        Position pos(message["x"], -1.5f, message["z"], message["angle"]);
                        map->addOpponent(id, pos, message["name"], message["vehicle"]);
                    }
                } else {
                    if (action == "leave-game") {
                        map->removeOpponent(id);
                    } else if (action == "move-player") {
                        Opponent* opponent = map->getOpponents().find(id)->second;
                        opponent->setPositionX(message["x"]);
                        opponent->setPositionZ(message["z"]);
                        opponent->setAngle(message["angle"]);
                    }
                }
            }
        }
        if (action == "pick-up-item") {
            Entity* item = nullptr;
            for (auto& entity : map->getEntities()) {
                if (entity->getPosition().x == message["x"] && entity->getPosition().z == message["z"]) {
                    item = entity;
                    break;
                }
            }
            if (item) {
                item->hide();
                if (message["id"] == playerId) {
                    if (typeid(*item) == typeid(FastPotion)) {
                        map->getPlayer()->setMaxVelocity(0.4f);
                        map->getPlayer()->activatePotion("Fast Potion", 5);
                    } else if (typeid(*item) == typeid(SlowPotion)) {
                        map->getPlayer()->setMaxVelocity(0.1f);
                        map->getPlayer()->activatePotion("Slow Potion", 10);
                    }
                }
            }
        }
    }
    if (action == "join-game") {
        pushChatMessage((std::string)message["name"] + " has connected to the game");
    } else if (action == "leave-game") {
        pushChatMessage((std::string)message["name"] + " has left the game");
    } else if (action == "chat-message") {
        pushChatMessage((std::string)message["name"] + ": " + (std::string)message["text"]);
    } else if (action == "win-game") {
        pushChatMessage((std::string)message["name"] + " has won the game");
    } else if (action == "pick-up-item") {
        pushChatMessage((std::string)message["name"] + " has picked up an item");
    } else if (action == "new-map") {
        std::string decodedData = base64_decode(message["map"]);
        std::string decompressedData = gzip::decompress(decodedData.c_str(), decodedData.size());
        players = message["players"];
        receivedMap.clear();
        receivedMap << decompressedData;
        receivedMapLoaded = false;
    } else if (action == "kick") {
        disconnectFromServer();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, nullptr, "You have been kicked out of the server", nullptr);
    } else if (action == "high-scores") {
        updateScores(message["scores"]);
    }
}

void Client::updateScores(const json& scores) {
    this->scores = scores;
    highScores.clear();
    for (auto& score : scores) {
        highScores.push_back(std::pair<std::string, int>(score["name"], score["score"]));
    }
    std::sort(highScores.begin(), highScores.end(), [](auto& left, auto& right) { return left.second > right.second; });
}

void Client::sendInitPlayer(const std::string& playerName, const std::string& vehicle) {
    json message;
    message["playerName"] = playerName;
    message["vehicle"] = vehicle;
    sendMessage(message);
}

void Client::sendPlayerPosition(Player* player) {
    if (player->getPosition().x == lastPlayerX && player->getPosition().z == lastPlayerZ && player->getAngle() == lastPlayerAngle) return;
    json message;
    message["action"] = "move-player";
    message["x"] = player->getPosition().x;
    message["z"] = player->getPosition().z;
    message["angle"] = player->getAngle();
    lastPlayerX = player->getPosition().x;
    lastPlayerZ = player->getPosition().z;
    lastPlayerAngle = player->getAngle();
    sendMessage(message);
}

void Client::sendChatMessage(const std::string& text) {
    json message;
    message["action"] = "chat-message";
    message["text"] = text;
    sendMessage(message);
}

std::string Client::popMessage() {
    std::string message = messageQueue.front();
    messageQueue.pop();
    return message;
}

void Client::clearQueue() {
    std::queue<std::string> emptyQueue;
    std::swap(messageQueue, emptyQueue);
}