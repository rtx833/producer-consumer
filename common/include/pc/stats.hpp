#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace pc {

// Packet and byte counts observed over a measured time span.
struct RateSample {
    std::uint64_t packets = 0;
    std::uint64_t bytes = 0;
    std::chrono::nanoseconds elapsed{0};

    [[nodiscard]] double seconds() const noexcept;
    [[nodiscard]] double packets_per_second() const noexcept;
    [[nodiscard]] double bytes_per_second() const noexcept;
};

// Accumulates counts since the last take(); rates use the real elapsed time
// rather than assuming an exact reporting period.
class RateWindow {
public:
    RateWindow() noexcept;

    void add(std::uint64_t bytes) noexcept {
        ++packets_;
        bytes_ += bytes;
    }
    [[nodiscard]] RateSample take() noexcept;
    void reset() noexcept;

private:
    std::uint64_t packets_ = 0;
    std::uint64_t bytes_ = 0;
    std::chrono::steady_clock::time_point start_;
};

// Fires once per period; the check itself is cheap enough to run per packet.
class PeriodicTimer {
public:
    explicit PeriodicTimer(std::chrono::milliseconds period) noexcept;
    [[nodiscard]] bool due() noexcept;

private:
    std::chrono::steady_clock::duration period_;
    std::chrono::steady_clock::time_point next_;
};

// Local wall-clock time as "HH:MM:SS".
[[nodiscard]] std::string local_time_hms();

}  // namespace pc
