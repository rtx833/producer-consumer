#include "pc/checksum.hpp"

#include <array>
#include <cstddef>

namespace pc {
namespace {

constexpr std::uint32_t kPolynomial = 0xEDB88320u;
constexpr std::size_t kSlices = 8;

using CrcTable = std::array<std::array<std::uint32_t, 256>, kSlices>;

constexpr CrcTable make_crc_table() {
    CrcTable table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (kPolynomial ^ (crc >> 1)) : (crc >> 1);
        }
        table[0][i] = crc;
    }
    for (std::uint32_t i = 0; i < 256; ++i) {
        for (std::size_t slice = 1; slice < kSlices; ++slice) {
            const std::uint32_t prev = table[slice - 1][i];
            table[slice][i] = (prev >> 8) ^ table[0][prev & 0xFFu];
        }
    }
    return table;
}

constexpr CrcTable kTable = make_crc_table();

constexpr std::uint32_t load_le32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::uint32_t Crc32::update(std::uint32_t crc, std::span<const std::uint8_t> data) const noexcept {
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();

    while (n >= 8) {
        const std::uint32_t one = load_le32(p) ^ crc;
        const std::uint32_t two = load_le32(p + 4);
        crc = kTable[0][(two >> 24) & 0xFFu] ^ kTable[1][(two >> 16) & 0xFFu] ^
              kTable[2][(two >> 8) & 0xFFu] ^ kTable[3][two & 0xFFu] ^
              kTable[4][(one >> 24) & 0xFFu] ^ kTable[5][(one >> 16) & 0xFFu] ^
              kTable[6][(one >> 8) & 0xFFu] ^ kTable[7][one & 0xFFu];
        p += 8;
        n -= 8;
    }
    while (n-- > 0) {
        crc = kTable[0][(crc ^ *p++) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

}  // namespace pc
