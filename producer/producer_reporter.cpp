#include "producer_reporter.hpp"

#include <format>

#include "pc/byte_size.hpp"

namespace pc {

void ConsoleProducerReporter::info(const std::string& message) {
    out_ << std::format("[{}] {}\n", local_time_hms(), message) << std::flush;
}

void ConsoleProducerReporter::state_changed(bool paused) {
    info(paused ? "** PAUSED  (any key / SIGUSR2 to resume)" : "** RESUMED (any key / SIGUSR1 to pause)");
}

void ConsoleProducerReporter::report(const ProducerSnapshot& s) {
    out_ << std::format("[{}] sent {} pkt ({}) | {} pkt/s {}/s | ring {:3.0f}% | {}\n", local_time_hms(),
                        format_count(s.total_packets), format_bytes(static_cast<double>(s.total_bytes)),
                        format_count(static_cast<std::uint64_t>(s.interval.packets_per_second())),
                        format_bytes(s.interval.bytes_per_second()), s.queue.fill_ratio() * 100.0,
                        s.paused ? "PAUSED" : "RUNNING")
         << std::flush;
}

void ConsoleProducerReporter::summary(std::uint64_t packets, std::uint64_t bytes, std::chrono::nanoseconds elapsed) {
    const RateSample overall{packets, bytes, elapsed};
    out_ << std::format("[{}] done: {} packets, {} in {:.2f} s (avg {} pkt/s, {}/s)\n", local_time_hms(),
                        format_count(packets), format_bytes(static_cast<double>(bytes)), overall.seconds(),
                        format_count(static_cast<std::uint64_t>(overall.packets_per_second())),
                        format_bytes(overall.bytes_per_second()))
         << std::flush;
}

}  // namespace pc
