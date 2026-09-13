#include "producer_app.hpp"

#include <format>
#include <stdexcept>
#include <thread>

#include "pc/packet.hpp"
#include "pc/stats.hpp"

namespace pc {

ProducerApp::ProducerApp(ProducerConfig config, IPacketSink& sink, PacketBuilder& builder, RunControl& control,
                         IProducerReporter& reporter) noexcept
    : config_(config), sink_(sink), builder_(builder), control_(control), reporter_(reporter) {}

int ProducerApp::run() {
    const std::size_t record_size = kHeaderSize + config_.payload_size;
    if (record_size > sink_.max_packet_size()) {
        throw std::invalid_argument(std::format("a {} byte packet does not fit into the transport (max {} bytes)",
                                                record_size, sink_.max_packet_size()));
    }
    reporter_.info(std::format("Transport: {}", sink_.describe()));
    reporter_.info(std::format("Packet: {} B header + {} B payload = {} B", kHeaderSize, config_.payload_size,
                               record_size));

    const std::stop_token stop = control_.stop_token();

    PeriodicTimer report_timer(config_.report_interval);
    RateWindow window;
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t sequence = 0;
    std::uint64_t total_bytes = 0;
    bool was_paused = false;

    const auto report_if_due = [&] {
        if (report_timer.due()) {
            reporter_.report({sequence, total_bytes, window.take(), sink_.queue_status(), was_paused});
        }
    };

    while (!stop.stop_requested()) {
        if (config_.max_packets != 0 && sequence >= config_.max_packets) {
            break;
        }
        const bool paused = control_.paused();
        if (paused != was_paused) {
            was_paused = paused;
            reporter_.state_changed(paused);
        }
        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            report_if_due();
            continue;
        }

        const auto slot = sink_.reserve(record_size, config_.reserve_timeout, stop);
        if (!slot) {
            report_if_due();  // queue full (consumer slow or paused) or stop requested
            continue;
        }
        builder_.build(*slot, sequence);
        sink_.commit();

        ++sequence;
        total_bytes += record_size;
        window.add(record_size);
        report_if_due();
    }

    reporter_.summary(sequence, total_bytes,
                      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started));
    return 0;
}

}  // namespace pc
