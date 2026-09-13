#include "pc/checksum.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <format>
#include <stdexcept>

#if defined(__x86_64__) || defined(__i386__)
#include <nmmintrin.h>
#define PC_HAVE_X86_CRC32C 1
#elif defined(__aarch64__)
#include <arm_acle.h>
#if defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
#define PC_HAVE_ARM_CRC32C 1
#endif

namespace pc {
namespace {

constexpr std::size_t kSlices = 8;
using CrcTable = std::array<std::array<std::uint32_t, 256>, kSlices>;

constexpr CrcTable make_crc_table(std::uint32_t polynomial) {
    CrcTable table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (polynomial ^ (crc >> 1)) : (crc >> 1);
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

constexpr CrcTable kCrc32Table = make_crc_table(0xEDB88320u);
constexpr CrcTable kCrc32cTable = make_crc_table(0x82F63B78u);

constexpr std::uint32_t load_le32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t crc_slice_by_8(const CrcTable& table, std::uint32_t crc, std::span<const std::uint8_t> data) noexcept {
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();
    while (n >= 8) {
        const std::uint32_t one = load_le32(p) ^ crc;
        const std::uint32_t two = load_le32(p + 4);
        crc = table[0][(two >> 24) & 0xFFu] ^ table[1][(two >> 16) & 0xFFu] ^ table[2][(two >> 8) & 0xFFu] ^
              table[3][two & 0xFFu] ^ table[4][(one >> 24) & 0xFFu] ^ table[5][(one >> 16) & 0xFFu] ^
              table[6][(one >> 8) & 0xFFu] ^ table[7][one & 0xFFu];
        p += 8;
        n -= 8;
    }
    while (n-- > 0) {
        crc = table[0][(crc ^ *p++) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

#if defined(PC_HAVE_X86_CRC32C)
__attribute__((target("sse4.2")))
std::uint32_t crc32c_hardware(std::uint32_t crc, std::span<const std::uint8_t> data) noexcept {
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();
    std::uint64_t wide = crc;
    while (n >= 8) {
        std::uint64_t word;
        std::memcpy(&word, p, sizeof word);
        wide = _mm_crc32_u64(wide, word);
        p += 8;
        n -= 8;
    }
    auto narrow = static_cast<std::uint32_t>(wide);
    while (n-- > 0) {
        narrow = _mm_crc32_u8(narrow, *p++);
    }
    return narrow;
}
#elif defined(PC_HAVE_ARM_CRC32C)
#if defined(__clang__)
__attribute__((target("crc")))
#else
__attribute__((target("+crc")))
#endif
std::uint32_t crc32c_hardware(std::uint32_t crc, std::span<const std::uint8_t> data) noexcept {
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();
    while (n >= 8) {
        std::uint64_t word;
        std::memcpy(&word, p, sizeof word);
        crc = __crc32cd(crc, word);
        p += 8;
        n -= 8;
    }
    while (n-- > 0) {
        crc = __crc32cb(crc, *p++);
    }
    return crc;
}
#endif

}  // namespace

// ---------------------------------------------------------------- CRC-32

std::uint32_t Crc32::update(std::uint32_t state, std::span<const std::uint8_t> data) const noexcept {
    return crc_slice_by_8(kCrc32Table, state, data);
}

// ---------------------------------------------------------------- CRC-32C

bool Crc32c::hardware_available() noexcept {
#if defined(PC_HAVE_X86_CRC32C)
    return __builtin_cpu_supports("sse4.2");
#elif defined(PC_HAVE_ARM_CRC32C) && defined(__linux__)
    return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0;
#elif defined(PC_HAVE_ARM_CRC32C) && defined(__APPLE__)
    return true;  // mandatory on every Apple arm64 CPU
#else
    return false;
#endif
}

Crc32c::Crc32c(bool allow_hardware) noexcept : hardware_(allow_hardware && hardware_available()) {}

std::uint32_t Crc32c::update(std::uint32_t state, std::span<const std::uint8_t> data) const noexcept {
#if defined(PC_HAVE_X86_CRC32C) || defined(PC_HAVE_ARM_CRC32C)
    if (hardware_) {
        return crc32c_hardware(state, data);
    }
#endif
    return crc_slice_by_8(kCrc32cTable, state, data);
}

// ---------------------------------------------------------------- registry / factory

const IChecksum* ChecksumRegistry::find(ChecksumKind kind) const noexcept {
    switch (kind) {
        case ChecksumKind::Crc32: return &crc32_;
        case ChecksumKind::Crc32c: return &crc32c_;
    }
    return nullptr;
}

std::unique_ptr<IChecksum> make_checksum(std::string_view name) {
    if (name == "crc32") {
        return std::make_unique<Crc32>();
    }
    if (name == "crc32c") {
        return std::make_unique<Crc32c>();
    }
    throw std::invalid_argument(std::format("unknown checksum '{}' (expected: crc32, crc32c)", name));
}

}  // namespace pc
