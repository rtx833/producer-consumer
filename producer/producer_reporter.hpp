#pragma once

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>

#include "pc/stats.hpp"
#include "pc/transport.hpp"

namespace pc {

struct ProducerSnapshot {
    std::uint64_t total_packets = 0;
    std::uint64_t total_bytes = 0;
    RateSample interval;
    QueueStatus queue;
    bool paused = false;
};

class IProducerReporter {
public:
    virtual ~IProducerReporter() = default;
    virtual void info(const std::string& message) = 0;
    virtual void state_changed(bool paused) = 0;
    virtual void report(const ProducerSnapshot& snapshot) = 0;
    virtual void summary(std::uint64_t packets, std::uint64_t bytes, std::chrono::nanoseconds elapsed) = 0;
};

class ConsoleProducerReporter final : public IProducerReporter {
public:
    explicit ConsoleProducerReporter(std::ostream& out) noexcept : out_(out) {}
    void info(const std::string& message) override;
    void state_changed(bool paused) override;
    void report(const ProducerSnapshot& snapshot) override;
    void summary(std::uint64_t packets, std::uint64_t bytes, std::chrono::nanoseconds elapsed) override;

private:
    std::ostream& out_;
};

}  // namespace pc
