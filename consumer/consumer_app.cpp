#include "consumer_app.hpp"

#include <format>

#include "pc/packet.hpp"
#include "pc/stats.hpp"

namespace pc {

ConsumerApp::ConsumerApp(ConsumerConfig config, IPacketSource& source, IPacketValidator& validator,
                         IPausePolicy& policy, RunControl& control, IConsumerReporter& reporter) noexcept
    : config_(config), source_(source), validator_(validator), policy_(policy), control_(control), reporter_(reporter) {}

int ConsumerApp::run() {
    const std::stop_token stop = control_.stop_token();
    reporter_.info(std::format("Pause policy '{}': {}", policy_.name(), policy_.description()));
    while (!stop.stop_requested()) {
        reporter_.info(std::format("Waiting for a producer on {} ...", source_.describe()));
        if (!source_.connect(stop)) {
            break;
        }
        reporter_.info(std::format("Connected to {}", source_.describe()));
        run_session(stop);
        source_.disconnect();
        reporter_.summary(stats_.totals(), stats_.elapsed());
        if (config_.once) {
            break;
        }
    }
    return 0;
}

void ConsumerApp::run_session(const std::stop_token& stop) {
    stats_.reset();
    validator_.resync();
    defect_logs_ = 0;
    PeriodicTimer report_timer(config_.report_interval);
    bool was_paused = false;

    const auto report_if_due = [&] {
        if (report_timer.due()) {
            reporter_.report(stats_.take_interval(), source_.queue_status(), was_paused);
        }
    };

    while (!stop.stop_requested()) {
        const bool paused = control_.paused();
        if (paused != was_paused) {
            was_paused = paused;
            reporter_.state_changed(paused);
            if (!paused) {
                policy_.on_resume(validator_);
            }
        }
        if (paused) {
            policy_.while_paused(source_, stats_, stop);
            report_if_due();
            continue;
        }

        if (const auto record = source_.receive(config_.receive_timeout, stop)) {
            process(*record);
        } else {
            // Nothing arrived within the timeout. Once the producer has ended
            // the stream (or died) no further packets can be published, so
            // one more non-blocking read decides whether the ring is drained.
            const PeerStatus peer = source_.peer_status();
            if (peer.finished || !peer.alive) {
                if (const auto last = source_.receive(std::chrono::milliseconds(0), stop)) {
                    process(*last);
                    continue;
                }
                reporter_.info(peer.finished ? "Producer finished the stream" : "Producer is gone");
                break;
            }
        }
        report_if_due();
    }
    source_.release();
}

void ConsumerApp::process(std::span<const std::uint8_t> record) {
    const ValidationResult result = validator_.validate(record, wall_clock_ns());
    stats_.record(result, record.size());
    if (!result.clean() && defect_logs_ < config_.max_defect_logs) {
        ++defect_logs_;
        reporter_.defect(result, record.size());
    }
}

}  // namespace pc
