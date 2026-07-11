#include <algorithm>
#include <cstdint>
#include <cstring>

#include "websocket.h"
#include "base64.h"

namespace {

// Compact SHA-1 (RFC 3174), only needed for the WebSocket accept key.
// Returns the raw 20-byte digest.
std::string sha1(const std::string& input) {
    uint32_t h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };

    // Pre-processing: append 0x80, pad with zeros, append 64-bit bit length.
    std::string data = input;
    const uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8u;
    data += static_cast<char>(0x80);
    while (data.size() % 64 != 56) data += '\0';
    for (int i = 7; i >= 0; --i) data += static_cast<char>((bitLength >> (i * 8)) & 0xFF);

    auto rol = [](uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); };

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + i * 4])) << 24)
                 | (static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + i * 4 + 1])) << 16)
                 | (static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + i * 4 + 2])) << 8)
                 | static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
            const uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    std::string digest;
    for (uint32_t v : h) {
        for (int i = 3; i >= 0; --i) digest += static_cast<char>((v >> (i * 8)) & 0xFF);
    }
    return digest;
}

// Case-insensitive lookup of an HTTP header value inside a raw request.
std::string headerValue(const std::string& request, const std::string& lowerName) {
    std::string lower = request;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string needle = "\r\n" + lowerName + ":";
    const size_t pos = lower.find(needle);
    if (pos == std::string::npos) return "";
    size_t start = pos + needle.size();
    size_t end = request.find("\r\n", start);
    if (end == std::string::npos) end = request.size();
    while (start < end && request[start] == ' ') ++start;
    size_t last = end;
    while (last > start && (request[last - 1] == ' ' || request[last - 1] == '\r')) --last;
    return request.substr(start, last - start);
}

bool sendAll(TCPsocket socket, const char* data, int length) {
    return SDLNet_TCP_Send(socket, data, length) >= length;
}

// Guard against absurd frame sizes (the whole protocol runs small JSON
// messages plus one compressed map); anything bigger is a broken client.
const uint64_t MAX_FRAME_PAYLOAD = 16u * 1024u * 1024u;
const size_t MAX_HANDSHAKE_SIZE = 16u * 1024u;

}  // namespace

int WebSocketTransport::detect(TCPsocket socket, std::string& initialData) {
    char buffer[4096];
    int received = SDLNet_TCP_Recv(socket, buffer, sizeof(buffer));
    if (received <= 0) return -1;

    // Native clients open with a JSON message ('{'); browsers with "GET ".
    // (A "GET " split across TCP segments is theoretically possible but the
    // handshake request always fits one segment in practice.)
    if (received < 4 || std::memcmp(buffer, "GET ", 4) != 0) {
        initialData.assign(buffer, static_cast<size_t>(received));
        return 0;
    }

    std::string request(buffer, static_cast<size_t>(received));
    while (request.find("\r\n\r\n") == std::string::npos) {
        if (request.size() > MAX_HANDSHAKE_SIZE) return -1;
        received = SDLNet_TCP_Recv(socket, buffer, sizeof(buffer));
        if (received <= 0) return -1;
        request.append(buffer, static_cast<size_t>(received));
    }

    const size_t headerEnd = request.find("\r\n\r\n") + 4;
    // Anything after the headers is already the first frame(s).
    frameBuffer = request.substr(headerEnd);
    request.resize(headerEnd);

    const std::string key = headerValue(request, "sec-websocket-key");
    if (key.empty()) return -1;
    const std::string accept = base64_encode(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n";
    // Browsers abort if they offered a subprotocol and none is echoed back;
    // the Emscripten client offers "binary".
    if (headerValue(request, "sec-websocket-protocol").find("binary") != std::string::npos) {
        response += "Sec-WebSocket-Protocol: binary\r\n";
    }
    response += "\r\n";
    if (!sendAll(socket, response.data(), static_cast<int>(response.size()))) return -1;
    return 1;
}

bool WebSocketTransport::parseFrames(TCPsocket socket) {
    size_t pos = 0;
    while (!closeReceived) {
        if (frameBuffer.size() - pos < 2) break;
        const unsigned char byte0 = static_cast<unsigned char>(frameBuffer[pos]);
        const unsigned char byte1 = static_cast<unsigned char>(frameBuffer[pos + 1]);
        const unsigned char opcode = byte0 & 0x0F;
        const bool masked = (byte1 & 0x80) != 0;
        uint64_t payloadLength = byte1 & 0x7F;
        size_t headerSize = 2;
        if (payloadLength == 126) {
            if (frameBuffer.size() - pos < headerSize + 2) break;
            payloadLength = (static_cast<uint64_t>(static_cast<unsigned char>(frameBuffer[pos + 2])) << 8)
                          | static_cast<unsigned char>(frameBuffer[pos + 3]);
            headerSize += 2;
        } else if (payloadLength == 127) {
            if (frameBuffer.size() - pos < headerSize + 8) break;
            payloadLength = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8) | static_cast<unsigned char>(frameBuffer[pos + 2 + i]);
            }
            headerSize += 8;
        }
        if (payloadLength > MAX_FRAME_PAYLOAD) return false;
        size_t maskOffset = pos + headerSize;
        if (masked) headerSize += 4;
        if (frameBuffer.size() - pos < headerSize + payloadLength) break;

        std::string payload = frameBuffer.substr(pos + headerSize, static_cast<size_t>(payloadLength));
        if (masked) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<char>(payload[i] ^ frameBuffer[maskOffset + (i % 4)]);
            }
        }

        switch (opcode) {
        case 0x0:  // continuation
        case 0x1:  // text
        case 0x2:  // binary
            decoded += payload;
            break;
        case 0x8:  // close: acknowledge, then report the connection as ended
            sendFrame(socket, 0x8, payload.data(), std::min<size_t>(payload.size(), 125));
            closeReceived = true;
            break;
        case 0x9:  // ping -> pong with the same payload
            sendFrame(socket, 0xA, payload.data(), payload.size());
            break;
        case 0xA:  // pong: ignore
            break;
        default:
            return false;
        }
        pos += headerSize + static_cast<size_t>(payloadLength);
    }
    frameBuffer.erase(0, pos);
    return true;
}

int WebSocketTransport::recvPayload(TCPsocket socket, char* out, int maxLength) {
    if (maxLength <= 0) return -1;
    // The handshake read may already have buffered frames.
    if (!parseFrames(socket)) return -1;
    while (decoded.empty()) {
        if (closeReceived) return 0;
        char buffer[4096];
        const int received = SDLNet_TCP_Recv(socket, buffer, sizeof(buffer));
        if (received <= 0) return received;
        frameBuffer.append(buffer, static_cast<size_t>(received));
        if (!parseFrames(socket)) return -1;
    }
    const int count = static_cast<int>(std::min<size_t>(decoded.size(), static_cast<size_t>(maxLength)));
    std::memcpy(out, decoded.data(), static_cast<size_t>(count));
    decoded.erase(0, static_cast<size_t>(count));
    return count;
}

bool WebSocketTransport::sendFrame(TCPsocket socket, unsigned char opcode, const char* data, size_t length) {
    std::string frame;
    frame += static_cast<char>(0x80 | opcode);  // FIN + opcode
    if (length < 126) {
        frame += static_cast<char>(length);
    } else if (length <= 0xFFFF) {
        frame += static_cast<char>(126);
        frame += static_cast<char>((length >> 8) & 0xFF);
        frame += static_cast<char>(length & 0xFF);
    } else {
        frame += static_cast<char>(127);
        for (int i = 7; i >= 0; --i) frame += static_cast<char>((static_cast<uint64_t>(length) >> (i * 8)) & 0xFF);
    }
    frame.append(data, length);
    return sendAll(socket, frame.data(), static_cast<int>(frame.size()));
}

bool WebSocketTransport::sendPayload(TCPsocket socket, const char* data, int length) {
    if (length < 0) return false;
    return sendFrame(socket, 0x2, data, static_cast<size_t>(length));
}
