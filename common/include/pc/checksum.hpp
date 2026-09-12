#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace pc {

// Incremental checksum algorithm. The incremental API lets callers hash a
// packet that is split across several memory regions (header + payload)
// without copying it into a temporary buffer.
class IChecksum {
public:
    virtual ~IChecksum() = default;

    [[nodiscard]] virtual std::uint32_t begin() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t update(std::uint32_t state,
                                               std::span<const std::uint8_t> data) const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t finish(std::uint32_t state) const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] std::uint32_t compute(std::span<const std::uint8_t> data) const noexcept {
        return finish(update(begin(), data));
    }
};

// CRC-32 (IEEE 802.3, reflected polynomial 0xEDB88320) using the slice-by-8
// technique: eight table lookups per eight input bytes instead of one per byte.
class Crc32 final : public IChecksum {
public:
    [[nodiscard]] std::uint32_t begin() const noexcept override { return 0xFFFFFFFFu; }
    [[nodiscard]] std::uint32_t update(std::uint32_t state,
                                       std::span<const std::uint8_t> data) const noexcept override;
    [[nodiscard]] std::uint32_t finish(std::uint32_t state) const noexcept override {
        return state ^ 0xFFFFFFFFu;
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "crc32"; }
};

}  // namespace pc
