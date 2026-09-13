#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace pc {

// Identifies the algorithm in the packet header so the consumer can verify
// packets with whatever checksum the producer chose.
enum class ChecksumKind : std::uint8_t {
    Crc32 = 1,   // IEEE 802.3, polynomial 0xEDB88320 (reflected)
    Crc32c = 2,  // Castagnoli, polynomial 0x82F63B78 (reflected); has CPU instructions
};

// Incremental checksum algorithm. The incremental API lets callers hash a
// packet that is split across several memory regions (header + payload)
// without copying it into a temporary buffer.
class IChecksum {
public:
    virtual ~IChecksum() = default;

    [[nodiscard]] virtual ChecksumKind kind() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t begin() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t update(std::uint32_t state,
                                               std::span<const std::uint8_t> data) const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t finish(std::uint32_t state) const noexcept = 0;

    [[nodiscard]] std::uint32_t compute(std::span<const std::uint8_t> data) const noexcept {
        return finish(update(begin(), data));
    }
};

// CRC-32 (IEEE 802.3) using the slice-by-8 table technique: eight table
// lookups per eight input bytes instead of one per byte.
class Crc32 final : public IChecksum {
public:
    [[nodiscard]] ChecksumKind kind() const noexcept override { return ChecksumKind::Crc32; }
    [[nodiscard]] std::string_view name() const noexcept override { return "crc32 (software, slice-by-8)"; }
    [[nodiscard]] std::uint32_t begin() const noexcept override { return 0xFFFFFFFFu; }
    [[nodiscard]] std::uint32_t update(std::uint32_t state,
                                       std::span<const std::uint8_t> data) const noexcept override;
    [[nodiscard]] std::uint32_t finish(std::uint32_t state) const noexcept override {
        return state ^ 0xFFFFFFFFu;
    }
};

// CRC-32C (Castagnoli). Uses the CPU's CRC32C instructions when they are
// available at run time (SSE4.2 on x86, the CRC extension on ARMv8) and the
// slice-by-8 table otherwise; both produce identical results.
class Crc32c final : public IChecksum {
public:
    explicit Crc32c(bool allow_hardware = true) noexcept;

    [[nodiscard]] static bool hardware_available() noexcept;
    [[nodiscard]] bool hardware() const noexcept { return hardware_; }

    [[nodiscard]] ChecksumKind kind() const noexcept override { return ChecksumKind::Crc32c; }
    [[nodiscard]] std::string_view name() const noexcept override {
        return hardware_ ? "crc32c (hardware)" : "crc32c (software, slice-by-8)";
    }
    [[nodiscard]] std::uint32_t begin() const noexcept override { return 0xFFFFFFFFu; }
    [[nodiscard]] std::uint32_t update(std::uint32_t state,
                                       std::span<const std::uint8_t> data) const noexcept override;
    [[nodiscard]] std::uint32_t finish(std::uint32_t state) const noexcept override {
        return state ^ 0xFFFFFFFFu;
    }

private:
    bool hardware_;
};

// Looks up the algorithm named in a packet header.
class IChecksumRegistry {
public:
    virtual ~IChecksumRegistry() = default;
    [[nodiscard]] virtual const IChecksum* find(ChecksumKind kind) const noexcept = 0;
};

// All algorithms this build knows about.
class ChecksumRegistry final : public IChecksumRegistry {
public:
    [[nodiscard]] const IChecksum* find(ChecksumKind kind) const noexcept override;

private:
    Crc32 crc32_;
    Crc32c crc32c_;
};

// Factory used by the producer's composition root; throws
// std::invalid_argument for unknown names. Registered names: "crc32", "crc32c".
std::unique_ptr<IChecksum> make_checksum(std::string_view name);

}  // namespace pc
