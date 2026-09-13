#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "payload_generator.hpp"
#include "pc/checksum.hpp"

namespace pc {

// Assembles one packet in place: header metadata (sequence number,
// wall-clock timestamp), payload from the generator, and the checksum.
//
// The payload is generated and hashed in chunks that fit into the L1 cache,
// so the checksum reads each chunk while it is still in cache instead of
// re-reading the whole payload from memory after it has been written.
class PacketBuilder {
public:
    static constexpr std::size_t kChunkBytes = 8 * 1024;

    PacketBuilder(IPayloadGenerator& generator, const IChecksum& checksum) noexcept
        : generator_(generator), checksum_(checksum) {}

    // Precondition: record.size() >= kHeaderSize.
    void build(std::span<std::uint8_t> record, std::uint64_t sequence) noexcept;

private:
    IPayloadGenerator& generator_;
    const IChecksum& checksum_;
};

}  // namespace pc
