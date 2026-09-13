#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "packet_builder.hpp"
#include "pc/control.hpp"
#include "pc/transport.hpp"
#include "producer_reporter.hpp"
#include "rate_limiter.hpp"

namespace pc {

struct ProducerConfig {
    std::size_t payload_size = 0;
    std::uint64_t max_packets = 0;  // 0 = unlimited
    std::chrono::milliseconds report_interval{1000};
    std::chrono::milliseconds reserve_timeout{50};
};

// Main loop of the producer: builds packets into the sink at the configured
// pace, honours pause/stop requests and reports progress periodically.
class ProducerApp {
public:
    ProducerApp(ProducerConfig config, IPacketSink& sink, PacketBuilder& builder, RateLimiter& limiter,
                RunControl& control, IProducerReporter& reporter) noexcept;

    int run();

private:
    ProducerConfig config_;
    IPacketSink& sink_;
    PacketBuilder& builder_;
    RateLimiter& limiter_;
    RunControl& control_;
    IProducerReporter& reporter_;
};

}  // namespace pc
