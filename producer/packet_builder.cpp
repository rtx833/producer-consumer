#include "packet_builder.hpp"

#include <algorithm>

#include "pc/packet.hpp"

namespace pc {

void PacketBuilder::build(std::span<std::uint8_t> record, std::uint64_t sequence) noexcept {
    write_header(record, sequence, wall_clock_ns(), checksum_.kind());
    std::uint32_t state = checksum_.update(checksum_.begin(), record.first(kHeaderSize));

    std::span<std::uint8_t> payload = record.subspan(kHeaderSize);
    while (!payload.empty()) {
        const std::span<std::uint8_t> chunk = payload.first(std::min(payload.size(), kChunkBytes));
        generator_.fill(chunk);
        state = checksum_.update(state, chunk);
        payload = payload.subspan(chunk.size());
    }
    patch_checksum(record, checksum_.finish(state));
}

}  // namespace pc
