#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <stop_token>

#include "consumer_reporter.hpp"
#include "consumer_stats.hpp"
#include "packet_validator.hpp"
#include "pause_policy.hpp"
#include "pc/control.hpp"
#include "pc/transport.hpp"

namespace pc {

struct ConsumerConfig {
    std::chrono::milliseconds report_interval{1000};
    std::chrono::milliseconds receive_timeout{50};
    unsigned max_defect_logs = 10;  // individual defect lines printed per session
    bool once = false;              // exit after the first producer session instead of waiting for the next
};

// Main loop of the consumer: attaches to a producer, validates every packet,
// applies the pause policy and reports statistics periodically. When the
// producer goes away it waits for the next one unless configured otherwise.
class ConsumerApp {
public:
    ConsumerApp(ConsumerConfig config, IPacketSource& source, IPacketValidator& validator, IPausePolicy& policy,
                RunControl& control, IConsumerReporter& reporter) noexcept;

    int run();

private:
    void run_session(const std::stop_token& stop);
    void process(std::span<const std::uint8_t> record);

    ConsumerConfig config_;
    IPacketSource& source_;
    IPacketValidator& validator_;
    IPausePolicy& policy_;
    RunControl& control_;
    IConsumerReporter& reporter_;
    ConsumerStats stats_;
    unsigned defect_logs_ = 0;
};

}  // namespace pc
