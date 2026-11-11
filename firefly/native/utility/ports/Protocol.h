/** Private utility port envelopes. IDs are native-issued capabilities within one child channel. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Firefly::Utility::Ports {
constexpr size_t header_size = 16;
constexpr size_t frame_limit = 128 * 1024;
constexpr uint32_t identity_limit = 0x7fffffff;
enum class Kind : uint8_t { Transfer = 1, Data, Close, Ack };

struct Identity {
    uint32_t id;
    uint32_t generation;
};

struct Packet {
    Kind kind;
    Identity route;
    std::vector<Identity> ports;
    std::span<char const> payload;
};

bool has_magic(std::span<char const>);

bool decode(std::span<char const>, Packet&);

std::vector<char> encode(Kind, Identity, std::span<Identity const>, std::span<char const>);

uint32_t read_size(std::span<char const>);

std::vector<char> size_payload(uint32_t);
}
