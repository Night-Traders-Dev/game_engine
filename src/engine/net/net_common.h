#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace eb {

// Packet types for basic multiplayer
enum class PacketType : uint8_t {
    PlayerMove = 1,
    PlayerState = 2,
    EntitySync = 3,
    Chat = 4,
    Ping = 5,
    Pong = 6,
    Connect = 10,
    Disconnect = 11,
};

struct NetPacket {
    PacketType type;
    uint32_t sequence;
    uint16_t data_size;
    uint8_t data[512];
};

// Placeholder for future UDP socket implementation
class NetSocket {
public:
    bool bind(uint16_t port) { (void)port; return false; /* TODO */ }
    bool connect(const std::string& host, uint16_t port) { (void)host; (void)port; return false; }
    int send(const void* data, int size) { (void)data; (void)size; return -1; }
    int recv(void* buf, int max_size) { (void)buf; (void)max_size; return -1; }
    void close() {}
    bool is_valid() const { return false; }
};

} // namespace eb
