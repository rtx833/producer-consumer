#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "check.hpp"
#include "packet_validator.hpp"
#include "pc/checksum.hpp"
#include "pc/packet.hpp"

namespace {

std::vector<std::uint8_t> make_packet(std::uint64_t sequence, std::int64_t timestamp_ns, std::size_t payload_size,
                                      const pc::IChecksum& checksum) {
    std::vector<std::uint8_t> record(pc::kHeaderSize + payload_size);
    for (std::size_t i = 0; i < payload_size; ++i) {
        record[pc::kHeaderSize + i] = static_cast<std::uint8_t>(i * 7 + sequence);
    }
    pc::seal_packet(record, sequence, timestamp_ns, checksum);
    return record;
}

}  // namespace

int main() {
    using pc::Defect;
    const pc::Crc32 crc;
    const pc::ChecksumRegistry registry;
    pc::PacketValidator validator(registry, std::chrono::seconds(1));
    const std::int64_t base_ns = 1'700'000'000'000'000'000;

    // A well-formed packet establishes the baseline and is clean.
    auto packet = make_packet(10, base_ns, 256, crc);
    auto result = validator.validate(packet, base_ns + 5'000);
    CHECK(result.clean());
    CHECK(result.sequence == 10);
    CHECK(result.latency_ns == 5'000);

    const auto header = pc::read_header(packet);
    CHECK(header.magic == pc::kPacketMagic);
    CHECK(header.payload_size == 256);
    CHECK(header.checksum == pc::packet_checksum(packet, crc));

    // Next in sequence with a later timestamp: clean.
    packet = make_packet(11, base_ns + 1'000, 256, crc);
    CHECK(validator.validate(packet, base_ns + 2'000).clean());

    // A flipped payload bit is a checksum failure and does not disturb sequence tracking.
    packet = make_packet(12, base_ns + 2'000, 256, crc);
    packet[pc::kHeaderSize + 100] ^= 0x01;
    result = validator.validate(packet, base_ns + 3'000);
    CHECK(result.has(Defect::BadChecksum));
    CHECK(result.corrupted());
    CHECK(!result.clean());

    packet = make_packet(12, base_ns + 2'000, 256, crc);
    CHECK(validator.validate(packet, base_ns + 3'000).clean());

    // Corrupted magic (and consequently checksum).
    packet = make_packet(13, base_ns + 3'000, 64, crc);
    packet[0] ^= 0xFF;
    result = validator.validate(packet, base_ns + 4'000);
    CHECK(result.has(Defect::BadMagic));
    CHECK(result.has(Defect::BadChecksum));

    // A record shorter than the header.
    std::vector<std::uint8_t> tiny(pc::kHeaderSize - 1);
    result = validator.validate(tiny, base_ns);
    CHECK(result.has(Defect::BadHeaderSize));
    CHECK(result.corrupted());

    // Sequence gap: 13 was corrupted, so 13 is still expected; 16 skips 13..15.
    packet = make_packet(16, base_ns + 4'000, 8, crc);
    result = validator.validate(packet, base_ns + 5'000);
    CHECK(result.has(Defect::SequenceGap));
    CHECK(result.missing == 3);
    CHECK(!result.corrupted());
    CHECK(result.anomalous());

    // Rewind and backwards timestamp.
    packet = make_packet(5, base_ns + 1'000, 8, crc);
    result = validator.validate(packet, base_ns + 6'000);
    CHECK(result.has(Defect::SequenceRewind));
    CHECK(result.has(Defect::TimestampBackward));

    // Timestamp too far in the future.
    packet = make_packet(6, base_ns + 5'000'000'000, 8, crc);
    result = validator.validate(packet, base_ns + 7'000);
    CHECK(result.has(Defect::TimestampFuture));

    // After resync() the next packet starts a new baseline: no gap reported.
    validator.resync();
    packet = make_packet(1000, base_ns + 8'000, 8, crc);
    CHECK(validator.validate(packet, base_ns + 9'000).clean());

    // A packet sealed with CRC-32C is verified with CRC-32C, chosen from its header.
    const pc::Crc32c crc32c;
    packet = make_packet(1001, base_ns + 9'000, 64, crc32c);
    CHECK(pc::read_header(packet).checksum_kind == static_cast<std::uint8_t>(pc::ChecksumKind::Crc32c));
    CHECK(validator.validate(packet, base_ns + 10'000).clean());

    // An unknown checksum algorithm makes the packet untrustworthy even if
    // the value itself is consistent.
    packet = make_packet(1002, base_ns + 10'000, 64, crc);
    packet[offsetof(pc::PacketHeader, checksum_kind)] = 200;
    const std::uint32_t reseal = pc::packet_checksum(packet, crc);
    std::memcpy(packet.data() + offsetof(pc::PacketHeader, checksum), &reseal, sizeof reseal);
    result = validator.validate(packet, base_ns + 11'000);
    CHECK(result.has(pc::Defect::UnknownChecksum));
    CHECK(result.corrupted());
    validator.resync();

    // Zero-length payload is a valid packet.
    packet = make_packet(1003, base_ns + 11'000, 0, crc);
    CHECK(validator.validate(packet, base_ns + 12'000).clean());

    CHECK(pc::describe_defects(0) == "ok");
    CHECK(pc::describe_defects(static_cast<unsigned>(Defect::BadChecksum) |
                               static_cast<unsigned>(Defect::SequenceGap)) == "bad checksum, sequence gap");
    return 0;
}
