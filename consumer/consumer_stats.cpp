#include "consumer_stats.hpp"

namespace pc {

void LatencySample::add(std::int64_t latency_ns) noexcept {
    if (latency_ns < min_ns) {
        min_ns = latency_ns;
    }
    if (latency_ns > max_ns) {
        max_ns = latency_ns;
    }
    sum_ns += latency_ns;
    ++count;
}

ConsumerStats::ConsumerStats() noexcept : started_(std::chrono::steady_clock::now()) {}

void ConsumerStats::record(const ValidationResult& result, std::size_t bytes) noexcept {
    ++totals_.received;
    totals_.bytes += bytes;
    window_.add(bytes);
    if (result.clean()) {
        ++totals_.valid;
    }
    if (result.corrupted()) {
        ++totals_.corrupted;
        return;
    }
    latency_.add(result.latency_ns);
    if (result.has(Defect::SequenceGap)) {
        ++totals_.sequence_gaps;
        totals_.missing += result.missing;
    }
    if (result.has(Defect::SequenceRewind)) {
        ++totals_.sequence_rewinds;
    }
    if (result.has(Defect::TimestampFuture) || result.has(Defect::TimestampBackward)) {
        ++totals_.timestamp_anomalies;
    }
}

void ConsumerStats::record_dropped(std::size_t bytes) noexcept {
    ++totals_.dropped;
    totals_.dropped_bytes += bytes;
}

ConsumerSnapshot ConsumerStats::take_interval() noexcept {
    ConsumerSnapshot snapshot{totals_, window_.take(), latency_};
    latency_ = LatencySample{};
    return snapshot;
}

std::chrono::nanoseconds ConsumerStats::elapsed() const noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started_);
}

void ConsumerStats::reset() noexcept {
    totals_ = ConsumerTotals{};
    window_.reset();
    latency_ = LatencySample{};
    started_ = std::chrono::steady_clock::now();
}

}  // namespace pc
