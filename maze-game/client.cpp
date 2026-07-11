#include <iostream>
#include <gzip/decompress.hpp>

#include "base64.h"
#include "client.h"
#include "map.h"

#if defined(__EMSCRIPTEN__)
#include <cstring>
#include <emscripten.h>
#include <emscripten/websocket.h>

namespace {
    // One connection at a time; every callback runs on the main thread (the
    // build is single-threaded), so plain fields need no locking. Callbacks
    // may fire while the wasm is suspended in emscripten_sleep — they only
    // record state, which is safe under Asyncify.
    struct WebSocketState {
        EMSCRIPTEN_WEBSOCKET_T handle = 0;
        std::string rxBuffer;
        bool open = false;
        bool failed = false;
        bool closed = false;
    };
    WebSocketState g_ws;

    EM_BOOL mazeWsOnOpen(int, const EmscriptenWebSocketOpenEvent*, void*) {
        g_ws.open = true;
        return EM_TRUE;
    }
    EM_BOOL mazeWsOnError(int, const EmscriptenWebSocketErrorEvent*, void*) {
        g_ws.failed = true;
        return EM_TRUE;
    }
    EM_BOOL mazeWsOnClose(int, const EmscriptenWebSocketCloseEvent*, void*) {
        g_ws.closed = true;
        g_ws.open = false;
        return EM_TRUE;
    }
    EM_BOOL mazeWsOnMessage(int, const EmscriptenWebSocketMessageEvent* event, void*) {
        if (event->isText) {
            // Text frames arrive NUL-terminated; numBytes includes the NUL.
            g_ws.rxBuffer.append(reinterpret_cast<const char*>(event->data),
                                 strlen(reinterpret_cast<const char*>(event->data)));
        } else {
            g_ws.rxBuffer.append(reinterpret_cast<const char*>(event->data), event->numBytes);
        }
        return EM_TRUE;
    }
}
#endif

Client::Client() {
    connected = false;
    receivedMapLoaded = false;
    map = nullptr;
#if !defined(__EMSCRIPTEN__)
    socket = nullptr;
    socketSet = SDLNet_AllocSocketSet(1);
#endif
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

void Client::disconnectFromServer() {
    // Only signal the worker thread to stop here. The socket must NOT be closed while
    // the worker may still be inside receiveMessages()/SDLNet_CheckSockets/SDLNet_TCP_Recv,
    // otherwise it operates on a freed socket and corrupts the heap. The socket is closed
    // by closeConnection() once the worker thread has been joined.
    // (On the web there is no worker thread, but the same ordering keeps
    // callers uniform: flip the flag here, tear the socket down later.)
    if (connected) {
        if (DEBUG_MODE) std::cout << "Disconnecting from the server..." << std::endl;
        connected = false;
    }
}

#if defined(__EMSCRIPTEN__)

void Client::pump() {
    if (!connected) return;
    receiveMessages(0);
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

void Client::connectToServer(const char* host, Uint16 port) {
    if (DEBUG_MODE) std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
    if (!emscripten_websocket_is_supported()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WebSocket", "This browser does not support WebSockets", nullptr);
        return;
    }
    closeConnection();
    g_ws = WebSocketState{};
    // A page served over https may only open secure WebSockets.
    const bool pageIsHttps = EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; }) != 0;
    const std::string url = std::string(pageIsHttps ? "wss://" : "ws://") + host + ":" + std::to_string(port) + "/";
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = url.c_str();
    attributes.protocols = "binary";  // echoed by maze-server's WebSocket handshake (websockify-compatible)
    EMSCRIPTEN_WEBSOCKET_T handle = emscripten_websocket_new(&attributes);
    if (handle <= 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WebSocket", "Could not create the WebSocket connection", nullptr);
        return;
    }
    g_ws.handle = handle;
    emscripten_websocket_set_onopen_callback(handle, nullptr, mazeWsOnOpen);
    emscripten_websocket_set_onerror_callback(handle, nullptr, mazeWsOnError);
    emscripten_websocket_set_onclose_callback(handle, nullptr, mazeWsOnClose);
    emscripten_websocket_set_onmessage_callback(handle, nullptr, mazeWsOnMessage);
    // Block (Asyncify) until the handshake settles, like SDLNet_TCP_Open does.
    const Uint32 start = SDL_GetTicks();
    while (!g_ws.open && !g_ws.failed && !g_ws.closed && SDL_GetTicks() - start < 5000) {
        emscripten_sleep(25);
    }
    if (!g_ws.open) {
        closeConnection();
        std::string text = "Could not connect to " + url + "\nIs maze-server running and reachable?";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Connection failed", text.c_str(), nullptr);
        return;
    }
    if (DEBUG_MODE) std::cout << "Successfully connected to " << url << "." << std::endl;
    clearQueue();
    highScores.clear();
    chatMessages.clear();
    connected = true;
}

void Client::closeConnection() {
    if (g_ws.handle > 0) {
        emscripten_websocket_close(g_ws.handle, 1000, "bye");
        emscripten_websocket_delete(g_ws.handle);
        g_ws.handle = 0;
    }
    g_ws.open = false;
}

void Client::sendMessage(const json& message) {
    if (!connected || !g_ws.open) return;
    std::string data = message.dump() + ';';
    if (emscripten_websocket_send_binary(g_ws.handle, const_cast<char*>(data.data()), data.size()) != EMSCRIPTEN_RESULT_SUCCESS) {
        if (DEBUG_MODE) std::cout << "emscripten_websocket_send_binary failed" << std::endl;
    }
    if (DEBUG_MODE) std::cout << "Sent message: " << data << std::endl;
}

void Client::receiveMessages(int timeout) {
    // The onmessage callback appends raw bytes to g_ws.rxBuffer; split complete
    // ';'-terminated messages into the queue and keep any partial tail.
    auto drain = [this]() {
        size_t end = g_ws.rxBuffer.rfind(';');
        if (end == std::string::npos) return;
        std::string buffer;
        for (size_t i = 0; i <= end; ++i) {
            char character = g_ws.rxBuffer[i];
            if (character != ';') {
                buffer += character;
            } else if (!buffer.empty()) {
                messageQueue.push(buffer);
                buffer.clear();
            }
        }
        g_ws.rxBuffer.erase(0, end + 1);
    };
    drain();
    if (timeout > 0) {
        const Uint32 start = SDL_GetTicks();
        while (isQueueEmpty() && !g_ws.closed && !g_ws.failed && SDL_GetTicks() - start < (Uint32)timeout) {
            emscripten_sleep(10);
            drain();
        }
    }
    if (connected && (g_ws.closed || g_ws.failed)) {
        disconnectFromServer();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, nullptr, "Server has stopped", nullptr);
    }
}

#else

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

void Client::closeConnection() {
    if (socket) {
        SDLNet_TCP_DelSocket(socketSet, socket);
        SDLNet_TCP_Close(socket);
        socket = nullptr;
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

#endif  // !defined(__EMSCRIPTEN__)

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