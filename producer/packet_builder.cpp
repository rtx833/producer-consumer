#include "packet_builder.hpp"

#include "pc/packet.hpp"

namespace pc {

void PacketBuilder::build(std::span<std::uint8_t> record, std::uint64_t sequence) noexcept {
    generator_.fill(record.subspan(kHeaderSize));
    seal_packet(record, sequence, wall_clock_ns(), checksum_);
}

}  // namespace pc
