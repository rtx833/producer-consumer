#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "pc/checksum.hpp"

namespace pc {

inline constexpr std::uint32_t kPacketMagic = 0x544B4350u;  // "PCKT" when read as little-endian bytes
inline constexpr std::uint8_t kPacketVersion = 2;

// Fixed-size metadata that precedes every payload. Both processes run on the
// same host, so native byte order and layout are used deliberately.
struct PacketHeader {
    std::uint32_t magic;
    std::uint8_t version;
    std::uint8_t checksum_kind;  // ChecksumKind used for `checksum`
    std::uint16_t header_size;
    std::uint32_t payload_size;
    std::uint32_t checksum;      // CRC over the whole packet with this field set to zero
    std::uint64_t sequence;      // 0, 1, 2, ... per producer run
    std::int64_t timestamp_ns;   // wall clock (CLOCK_REALTIME) nanoseconds since the epoch
};

inline constexpr std::size_t kHeaderSize = sizeof(PacketHeader);
static_assert(kHeaderSize == 32);
static_assert(std::is_trivially_copyable_v<PacketHeader>);
static_assert(std::is_standard_layout_v<PacketHeader>);

// Fills in the header of a packet whose payload already occupies
// record[kHeaderSize..] and stamps the checksum over the whole record.
void seal_packet(std::span<std::uint8_t> record, std::uint64_t sequence, std::int64_t timestamp_ns,
                 const IChecksum& checksum) noexcept;

// Copies the header out of a raw record (the record may be unaligned).
// Precondition: record.size() >= kHeaderSize.
[[nodiscard]] PacketHeader read_header(std::span<const std::uint8_t> record) noexcept;

// Recomputes the checksum of a raw record the way seal_packet() produced it.
[[nodiscard]] std::uint32_t packet_checksum(std::span<const std::uint8_t> record,
                                            const IChecksum& checksum) noexcept;

[[nodiscard]] std::int64_t wall_clock_ns() noexcept;

}  // namespace pc
