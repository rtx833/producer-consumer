#include "pc/packet.hpp"

#include <chrono>
#include <cstring>

namespace pc {

void write_header(std::span<std::uint8_t> record, std::uint64_t sequence, std::int64_t timestamp_ns,
                  ChecksumKind checksum_kind) noexcept {
    PacketHeader header{};
    header.magic = kPacketMagic;
    header.version = kPacketVersion;
    header.checksum_kind = static_cast<std::uint8_t>(checksum_kind);
    header.header_size = static_cast<std::uint16_t>(kHeaderSize);
    header.payload_size = static_cast<std::uint32_t>(record.size() - kHeaderSize);
    header.checksum = 0;
    header.sequence = sequence;
    header.timestamp_ns = timestamp_ns;
    std::memcpy(record.data(), &header, kHeaderSize);
}

void patch_checksum(std::span<std::uint8_t> record, std::uint32_t checksum) noexcept {
    std::memcpy(record.data() + offsetof(PacketHeader, checksum), &checksum, sizeof checksum);
}

void seal_packet(std::span<std::uint8_t> record, std::uint64_t sequence, std::int64_t timestamp_ns,
                 const IChecksum& checksum) noexcept {
    write_header(record, sequence, timestamp_ns, checksum.kind());
    patch_checksum(record, checksum.compute(record));
}

PacketHeader read_header(std::span<const std::uint8_t> record) noexcept {
    PacketHeader header;
    std::memcpy(&header, record.data(), kHeaderSize);
    return header;
}

std::uint32_t packet_checksum(std::span<const std::uint8_t> record, const IChecksum& checksum) noexcept {
    // Hash a private copy of the header with the checksum field cleared, then
    // continue over the payload in place: no copy of the payload is made.
    PacketHeader header = read_header(record);
    header.checksum = 0;
    std::uint8_t header_bytes[kHeaderSize];
    std::memcpy(header_bytes, &header, kHeaderSize);

    std::uint32_t state = checksum.begin();
    state = checksum.update(state, std::span<const std::uint8_t>(header_bytes, kHeaderSize));
    state = checksum.update(state, record.subspan(kHeaderSize));
    return checksum.finish(state);
}

std::int64_t wall_clock_ns() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

}  // namespace pc
