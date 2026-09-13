#pragma once

#include <cstdint>
#include <span>

#include "payload_generator.hpp"
#include "pc/checksum.hpp"

namespace pc {

// Assembles one packet in place: payload from the generator, then header
// metadata (sequence number, wall-clock timestamp, checksum).
class PacketBuilder {
public:
    PacketBuilder(IPayloadGenerator& generator, const IChecksum& checksum) noexcept
        : generator_(generator), checksum_(checksum) {}

    // Precondition: record.size() >= kHeaderSize.
    void build(std::span<std::uint8_t> record, std::uint64_t sequence) noexcept;

private:
    IPayloadGenerator& generator_;
    const IChecksum& checksum_;
};

}  // namespace pc
