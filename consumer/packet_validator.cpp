#include "packet_validator.hpp"

#include "pc/packet.hpp"

namespace pc {

std::string describe_defects(unsigned defects) {
    static constexpr struct {
        Defect defect;
        const char* text;
    } kNames[] = {
        {Defect::BadMagic, "bad magic"},
        {Defect::BadVersion, "bad version"},
        {Defect::BadHeaderSize, "bad header size"},
        {Defect::SizeMismatch, "size mismatch"},
        {Defect::BadChecksum, "bad checksum"},
        {Defect::SequenceGap, "sequence gap"},
        {Defect::SequenceRewind, "sequence rewind"},
        {Defect::TimestampFuture, "timestamp in the future"},
        {Defect::TimestampBackward, "timestamp went backwards"},
    };
    std::string text;
    for (const auto& [defect, name] : kNames) {
        if ((defects & static_cast<unsigned>(defect)) != 0) {
            if (!text.empty()) {
                text += ", ";
            }
            text += name;
        }
    }
    return text.empty() ? "ok" : text;
}

PacketValidator::PacketValidator(const IChecksum& checksum, std::chrono::nanoseconds future_tolerance) noexcept
    : checksum_(checksum), future_tolerance_ns_(future_tolerance.count()) {}

ValidationResult PacketValidator::validate(std::span<const std::uint8_t> record, std::int64_t now_ns) {
    ValidationResult result;
    if (record.size() < kHeaderSize) {
        result.defects |= static_cast<unsigned>(Defect::BadHeaderSize) | static_cast<unsigned>(Defect::SizeMismatch);
        return result;
    }

    const PacketHeader header = read_header(record);
    result.sequence = header.sequence;
    if (header.magic != kPacketMagic) {
        result.defects |= static_cast<unsigned>(Defect::BadMagic);
    }
    if (header.version != kPacketVersion) {
        result.defects |= static_cast<unsigned>(Defect::BadVersion);
    }
    if (header.header_size != kHeaderSize) {
        result.defects |= static_cast<unsigned>(Defect::BadHeaderSize);
    }
    if (header.payload_size != record.size() - kHeaderSize) {
        result.defects |= static_cast<unsigned>(Defect::SizeMismatch);
    }
    if (packet_checksum(record, checksum_) != header.checksum) {
        result.defects |= static_cast<unsigned>(Defect::BadChecksum);
    }
    if (result.corrupted()) {
        return result;  // metadata is not trustworthy; leave sequence tracking untouched
    }

    result.latency_ns = now_ns - header.timestamp_ns;
    if (header.timestamp_ns > now_ns + future_tolerance_ns_) {
        result.defects |= static_cast<unsigned>(Defect::TimestampFuture);
    }
    if (has_baseline_) {
        if (header.sequence > expected_sequence_) {
            result.defects |= static_cast<unsigned>(Defect::SequenceGap);
            result.missing = header.sequence - expected_sequence_;
        } else if (header.sequence < expected_sequence_) {
            result.defects |= static_cast<unsigned>(Defect::SequenceRewind);
        }
        if (header.timestamp_ns < last_timestamp_ns_) {
            result.defects |= static_cast<unsigned>(Defect::TimestampBackward);
        }
    }
    has_baseline_ = true;
    expected_sequence_ = header.sequence + 1;
    last_timestamp_ns_ = header.timestamp_ns;
    return result;
}

}  // namespace pc
