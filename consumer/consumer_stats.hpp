#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "packet_validator.hpp"
#include "pc/stats.hpp"

namespace pc {

struct ConsumerTotals {
    std::uint64_t received = 0;   // packets taken from the transport and validated
    std::uint64_t bytes = 0;      // header + payload bytes of received packets
    std::uint64_t valid = 0;      // no defects at all
    std::uint64_t corrupted = 0;  // structure or checksum failures
    std::uint64_t sequence_gaps = 0;
    std::uint64_t missing = 0;    // packets skipped according to sequence numbers
    std::uint64_t sequence_rewinds = 0;
    std::uint64_t timestamp_anomalies = 0;
    std::uint64_t dropped = 0;    // discarded while paused (discard policy)
    std::uint64_t dropped_bytes = 0;
};

struct LatencySample {
    std::int64_t min_ns = std::numeric_limits<std::int64_t>::max();
    std::int64_t max_ns = std::numeric_limits<std::int64_t>::min();
    std::int64_t sum_ns = 0;
    std::uint64_t count = 0;

    [[nodiscard]] bool empty() const noexcept { return count == 0; }
    [[nodiscard]] double average_ns() const noexcept {
        return count == 0 ? 0.0 : static_cast<double>(sum_ns) / static_cast<double>(count);
    }
    void add(std::int64_t latency_ns) noexcept;
};

struct ConsumerSnapshot {
    ConsumerTotals totals;
    RateSample interval;     // received packets during the last window
    LatencySample latency;   // over uncorrupted packets of the last window
};

class ConsumerStats {
public:
    ConsumerStats() noexcept;

    void record(const ValidationResult& result, std::size_t bytes) noexcept;
    void record_dropped(std::size_t bytes) noexcept;

    [[nodiscard]] ConsumerSnapshot take_interval() noexcept;
    [[nodiscard]] const ConsumerTotals& totals() const noexcept { return totals_; }
    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept;
    void reset() noexcept;

private:
    ConsumerTotals totals_;
    RateWindow window_;
    LatencySample latency_;
    std::chrono::steady_clock::time_point started_;
};

}  // namespace pc
