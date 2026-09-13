#pragma once

#include <chrono>
#include <thread>

namespace pc {

inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

// Progressive waiting strategy for lock-free queues: spin briefly (lowest
// latency while the peer is active), then yield the CPU, then sleep with a
// growing interval capped at 1 ms (no busy loop while the peer is idle).
class Backoff {
public:
    void wait() noexcept {
        if (step_ < kSpinSteps) {
            cpu_relax();
        } else if (step_ < kSpinSteps + kYieldSteps) {
            std::this_thread::yield();
        } else {
            const auto exponent = step_ - kSpinSteps - kYieldSteps;
            auto delay = std::chrono::microseconds(50) * (1u << (exponent < 5 ? exponent : 4));
            if (delay > kMaxSleep) {
                delay = kMaxSleep;
            }
            std::this_thread::sleep_for(delay);
        }
        if (step_ < kSpinSteps + kYieldSteps + 8) {
            ++step_;
        }
    }

    // True once the strategy has reached the sleeping phase; callers use it
    // to avoid reading the clock on the hot spinning path.
    [[nodiscard]] bool sleeping() const noexcept { return step_ >= kSpinSteps + kYieldSteps; }
    void reset() noexcept { step_ = 0; }

private:
    static constexpr unsigned kSpinSteps = 64;
    static constexpr unsigned kYieldSteps = 16;
    static constexpr std::chrono::microseconds kMaxSleep{1000};
    unsigned step_ = 0;
};

}  // namespace pc
