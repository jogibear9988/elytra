/** Fixed-width, endian-independent framing; legacy utility data remains outside this envelope. */
#include "Protocol.h"
#include <algorithm>

namespace Firefly::Utility::Ports {
namespace {
    uint32_t read32(std::span<char const> bytes, size_t offset)
    {
        uint32_t result = 0;
        for (size_t index = 0; index < 4; ++index)
            result = (result << 8) | static_cast<uint8_t>(bytes[offset + index]);
        return result;
    }

    void write32(std::vector<char>& bytes, size_t offset, uint32_t value)
    {
        for (size_t index = 0; index < 4; ++index)
            bytes[offset + index] = static_cast<char>(value >> (24 - index * 8));
    }

    bool valid(Identity identity)
    {
        return identity.id && identity.id <= identity_limit && identity.generation
            && identity.generation <= identity_limit;
    }
}

bool has_magic(std::span<char const> bytes)
{
    return bytes.size() >= 4 && bytes[0] == 'F' && bytes[1] == 'U' && bytes[2] == 'P'
        && bytes[3] == '1';
}

bool decode(std::span<char const> bytes, Packet& packet)
{
    if (!has_magic(bytes) || bytes.size() < header_size || bytes.size() > frame_limit || bytes[6]
        || bytes[7])
        return false;
    auto kind = static_cast<uint8_t>(bytes[4]);
    if (kind < 1 || kind > 4)
        return false;
    auto count = static_cast<uint8_t>(bytes[5]);
    Identity route { read32(bytes, 8), read32(bytes, 12) };
    if (kind == static_cast<uint8_t>(Kind::Transfer)) {
        if (!count || count > 16 || route.id || route.generation
            || bytes.size() < header_size + count * 8)
            return false;
    } else if (count || !valid(route)) {
        return false;
    }

    packet = { static_cast<Kind>(kind), route, {}, {} };
    for (size_t index = 0; index < count; ++index) {
        Identity item { read32(bytes, header_size + index * 8),
            read32(bytes, header_size + index * 8 + 4) };
        if (!valid(item)
            || std::ranges::any_of(packet.ports, [&](auto prior) { return prior.id == item.id; }))
            return false;
        packet.ports.push_back(item);
    }

    packet.payload = bytes.subspan(header_size + count * 8);
    if (((packet.kind == Kind::Transfer || packet.kind == Kind::Data) && packet.payload.empty())
        || (packet.kind == Kind::Close && !packet.payload.empty())
        || (packet.kind == Kind::Ack && packet.payload.size() != 4))
        return false;
    return true;
}

std::vector<char> encode(
    Kind kind, Identity route, std::span<Identity const> ports, std::span<char const> payload)
{
    std::vector<char> bytes(header_size + ports.size() * 8 + payload.size());
    bytes[0] = 'F';
    bytes[1] = 'U';
    bytes[2] = 'P';
    bytes[3] = '1';
    bytes[4] = static_cast<char>(kind);
    bytes[5] = static_cast<char>(ports.size());
    write32(bytes, 8, route.id);
    write32(bytes, 12, route.generation);
    for (size_t index = 0; index < ports.size(); ++index) {
        write32(bytes, header_size + index * 8, ports[index].id);
        write32(bytes, header_size + index * 8 + 4, ports[index].generation);
    }

    std::ranges::copy(payload, bytes.begin() + header_size + ports.size() * 8);
    return bytes;
}

uint32_t read_size(std::span<char const> bytes)
{
    return read32(bytes, 0);
}

std::vector<char> size_payload(uint32_t size)
{
    std::vector<char> bytes(4);
    write32(bytes, 0, size);
    return bytes;
}
}
