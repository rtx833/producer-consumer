#pragma once

#include <chrono>
#include <stop_token>

namespace pc {

// Paces packet production at a fixed number of packets per second. Sleeps
// for the bulk of each interval and spins only for the last stretch to stay
// precise at high rates. A rate of zero disables pacing.
class RateLimiter {
public:
    explicit RateLimiter(double packets_per_second) noexcept;

    [[nodiscard]] bool enabled() const noexcept { return interval_.count() > 0; }
    void wait_for_slot(std::stop_token stop) noexcept;

private:
    using Clock = std::chrono::steady_clock;
    Clock::duration interval_{0};
    Clock::time_point next_;
};

}  // namespace pc
