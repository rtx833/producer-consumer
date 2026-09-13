#include "consumer_reporter.hpp"

#include <format>

#include "pc/byte_size.hpp"

namespace pc {

void ConsoleConsumerReporter::info(const std::string& message) {
    out_ << std::format("[{}] {}\n", local_time_hms(), message) << std::flush;
}

void ConsoleConsumerReporter::state_changed(bool paused) {
    info(paused ? "** PAUSED  (any key / SIGUSR2 to resume)" : "** RESUMED (any key / SIGUSR1 to pause)");
}

void ConsoleConsumerReporter::report(const ConsumerSnapshot& s, const QueueStatus& queue, bool paused) {
    std::string latency = "n/a";
    if (!s.latency.empty()) {
        latency = std::format("avg {} max {}", format_duration_ns(static_cast<std::int64_t>(s.latency.average_ns())),
                              format_duration_ns(s.latency.max_ns));
    }
    std::string problems;
    if (s.totals.corrupted != 0) {
        problems += std::format(" | corrupted {}", format_count(s.totals.corrupted));
    }
    if (s.totals.missing != 0 || s.totals.sequence_rewinds != 0) {
        problems += std::format(" | missing {} rewinds {}", format_count(s.totals.missing),
                                format_count(s.totals.sequence_rewinds));
    }
    if (s.totals.timestamp_anomalies != 0) {
        problems += std::format(" | ts-anomalies {}", format_count(s.totals.timestamp_anomalies));
    }
    if (s.totals.dropped != 0) {
        problems += std::format(" | dropped {}", format_count(s.totals.dropped));
    }
    out_ << std::format("[{}] rx {} pkt ({}) | {} pkt/s {}/s | latency {} | ring {:3.0f}%{} | {}\n",
                        local_time_hms(), format_count(s.totals.received),
                        format_bytes(static_cast<double>(s.totals.bytes)),
                        format_count(static_cast<std::uint64_t>(s.interval.packets_per_second())),
                        format_bytes(s.interval.bytes_per_second()), latency, queue.fill_ratio() * 100.0, problems,
                        paused ? "PAUSED" : "RUNNING")
         << std::flush;
}

void ConsoleConsumerReporter::defect(const ValidationResult& result, std::size_t bytes) {
    std::string extra;
    if (result.has(Defect::SequenceGap)) {
        extra = std::format(", {} packet(s) missing", format_count(result.missing));
    }
    info(std::format("!! packet #{} ({} B): {}{}", result.sequence, bytes, describe_defects(result.defects), extra));
}

void ConsoleConsumerReporter::summary(const ConsumerTotals& t, std::chrono::nanoseconds elapsed) {
    const RateSample overall{t.received, t.bytes, elapsed};
    info(std::format("session summary: received {} packets ({}) in {:.2f} s, avg {} pkt/s {}/s",
                     format_count(t.received), format_bytes(static_cast<double>(t.bytes)), overall.seconds(),
                     format_count(static_cast<std::uint64_t>(overall.packets_per_second())),
                     format_bytes(overall.bytes_per_second())));
    info(std::format("  valid {} | corrupted {} | sequence gaps {} (missing {}) | rewinds {} | "
                     "timestamp anomalies {} | dropped while paused {} ({})",
                     format_count(t.valid), format_count(t.corrupted), format_count(t.sequence_gaps),
                     format_count(t.missing), format_count(t.sequence_rewinds), format_count(t.timestamp_anomalies),
                     format_count(t.dropped), format_bytes(static_cast<double>(t.dropped_bytes))));
}

}  // namespace pc
