#include "pc/stats.hpp"

#include <ctime>

namespace pc {

double RateSample::seconds() const noexcept {
    return std::chrono::duration<double>(elapsed).count();
}

double RateSample::packets_per_second() const noexcept {
    const double s = seconds();
    return s > 0.0 ? static_cast<double>(packets) / s : 0.0;
}

double RateSample::bytes_per_second() const noexcept {
    const double s = seconds();
    return s > 0.0 ? static_cast<double>(bytes) / s : 0.0;
}

RateWindow::RateWindow() noexcept : start_(std::chrono::steady_clock::now()) {}

RateSample RateWindow::take() noexcept {
    const auto now = std::chrono::steady_clock::now();
    RateSample sample{packets_, bytes_, std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_)};
    packets_ = 0;
    bytes_ = 0;
    start_ = now;
    return sample;
}

void RateWindow::reset() noexcept {
    packets_ = 0;
    bytes_ = 0;
    start_ = std::chrono::steady_clock::now();
}

PeriodicTimer::PeriodicTimer(std::chrono::milliseconds period) noexcept
    : period_(period), next_(std::chrono::steady_clock::now() + period) {}

bool PeriodicTimer::due() noexcept {
    const auto now = std::chrono::steady_clock::now();
    if (now < next_) {
        return false;
    }
    next_ = now + period_;
    return true;
}

std::string local_time_hms() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char buffer[16];
    const std::size_t n = std::strftime(buffer, sizeof buffer, "%H:%M:%S", &local);
    return std::string(buffer, n);
}

}  // namespace pc
