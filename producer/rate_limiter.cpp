#include "rate_limiter.hpp"

#include <thread>

#include "pc/backoff.hpp"

namespace pc {

RateLimiter::RateLimiter(double packets_per_second) noexcept : next_(Clock::now()) {
    if (packets_per_second > 0.0) {
        interval_ = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / packets_per_second));
        if (interval_.count() == 0) {
            interval_ = Clock::duration(1);
        }
    }
}

void RateLimiter::wait_for_slot(std::stop_token stop) noexcept {
    if (!enabled()) {
        return;
    }
    constexpr auto kSpinThreshold = std::chrono::microseconds(200);
    constexpr auto kMaxSleep = std::chrono::milliseconds(20);

    auto now = Clock::now();
    while (now < next_ && !stop.stop_requested()) {
        const auto remaining = next_ - now;
        if (remaining > kSpinThreshold) {
            auto nap = remaining - kSpinThreshold / 2;
            std::this_thread::sleep_for(nap < kMaxSleep ? nap : kMaxSleep);
        } else {
            cpu_relax();
        }
        now = Clock::now();
    }
    // Keep a fixed cadence while on time; after a stall (pause, full queue)
    // restart from now instead of bursting to catch up.
    next_ = (now - next_ < interval_) ? next_ + interval_ : now + interval_;
}

}  // namespace pc
