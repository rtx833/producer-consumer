#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>

#include "pc/checksum.hpp"

namespace pc {

enum class Defect : unsigned {
    BadMagic = 1u << 0,
    BadVersion = 1u << 1,
    BadHeaderSize = 1u << 2,
    SizeMismatch = 1u << 3,  // header payload_size disagrees with the record length
    BadChecksum = 1u << 4,
    SequenceGap = 1u << 5,        // packets were skipped
    SequenceRewind = 1u << 6,     // duplicate or out-of-order sequence number
    TimestampFuture = 1u << 7,    // timestamp ahead of the local clock beyond tolerance
    TimestampBackward = 1u << 8,  // timestamp earlier than the previous packet's
    UnknownChecksum = 1u << 9,    // header names a checksum algorithm this build does not know
};

// Defects that mean the packet content cannot be trusted at all.
inline constexpr unsigned kCorruptionDefects =
    static_cast<unsigned>(Defect::BadMagic) | static_cast<unsigned>(Defect::BadVersion) |
    static_cast<unsigned>(Defect::BadHeaderSize) | static_cast<unsigned>(Defect::SizeMismatch) |
    static_cast<unsigned>(Defect::BadChecksum) | static_cast<unsigned>(Defect::UnknownChecksum);

struct ValidationResult {
    unsigned defects = 0;
    std::uint64_t sequence = 0;
    std::uint64_t missing = 0;     // packets skipped when SequenceGap is set
    std::int64_t latency_ns = 0;   // receive time minus packet timestamp (uncorrupted packets only)

    [[nodiscard]] bool has(Defect defect) const noexcept { return (defects & static_cast<unsigned>(defect)) != 0; }
    [[nodiscard]] bool corrupted() const noexcept { return (defects & kCorruptionDefects) != 0; }
    [[nodiscard]] bool anomalous() const noexcept { return (defects & ~kCorruptionDefects) != 0; }
    [[nodiscard]] bool clean() const noexcept { return defects == 0; }
};

[[nodiscard]] std::string describe_defects(unsigned defects);

class IPacketValidator {
public:
    virtual ~IPacketValidator() = default;
    // `now_ns` is the wall-clock receive time, on the same clock as the packet timestamp.
    [[nodiscard]] virtual ValidationResult validate(std::span<const std::uint8_t> record, std::int64_t now_ns) = 0;
    // Forgets sequence/timestamp expectations; the next packet starts a new baseline.
    virtual void resync() noexcept = 0;
};

// Checks structure, checksum, sequence continuity and timestamp sanity. The
// checksum algorithm is taken from each packet's header.
class PacketValidator final : public IPacketValidator {
public:
    explicit PacketValidator(const IChecksumRegistry& checksums,
                             std::chrono::nanoseconds future_tolerance = std::chrono::seconds(1)) noexcept;

    [[nodiscard]] ValidationResult validate(std::span<const std::uint8_t> record, std::int64_t now_ns) override;
    void resync() noexcept override { has_baseline_ = false; }

private:
    const IChecksumRegistry& checksums_;
    std::int64_t future_tolerance_ns_;
    bool has_baseline_ = false;
    std::uint64_t expected_sequence_ = 0;
    std::int64_t last_timestamp_ns_ = 0;
};

}  // namespace pc
