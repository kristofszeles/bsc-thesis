#pragma once

#include <string>
#include <SDL_net.h>

// Minimal RFC 6455 WebSocket server transport, so browsers running the
// Emscripten web port of maze-game can connect directly to the game port —
// no websockify bridge. Native TCP clients are unaffected: the first bytes a
// WebSocket client sends are an HTTP upgrade request ("GET ... HTTP/1.1"),
// while native clients start with a JSON message ("{...};"), so the two are
// told apart by peeking at the first read.
//
// One instance per client connection (owned by its ServerThread); the parsing
// state is not thread-safe, matching the one-reader-thread-per-client model.
class WebSocketTransport {
public:
    // Reads the first bytes from the socket and decides what the client is.
    // Returns:
    //    1  WebSocket client; the HTTP handshake has been completed.
    //    0  native TCP client; the consumed bytes are returned in initialData
    //       and must be treated as regular game traffic.
    //   -1  the connection closed or the handshake failed.
    int detect(TCPsocket socket, std::string& initialData);

    // Receive decoded application bytes. Handles masking, fragmentation and
    // control frames (ping is answered, close is acknowledged) internally.
    // Semantics match SDLNet_TCP_Recv: >0 bytes written to out, <=0 closed.
    int recvPayload(TCPsocket socket, char* out, int maxLength);

    // Send data to the client wrapped in a single binary frame.
    bool sendPayload(TCPsocket socket, const char* data, int length);

private:
    std::string frameBuffer;  // raw bytes received but not yet parsed into frames
    std::string decoded;      // unmasked payload not yet handed to the caller
    bool closeReceived = false;

    // Parse every complete frame in frameBuffer into decoded; answers control
    // frames on the socket. Returns false on a protocol violation.
    bool parseFrames(TCPsocket socket);
    static bool sendFrame(TCPsocket socket, unsigned char opcode, const char* data, size_t length);
};
