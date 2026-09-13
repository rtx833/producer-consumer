#pragma once

#include <chrono>
#include <cstddef>
#include <ostream>
#include <string>

#include "consumer_stats.hpp"
#include "packet_validator.hpp"
#include "pc/transport.hpp"

namespace pc {

class IConsumerReporter {
public:
    virtual ~IConsumerReporter() = default;
    virtual void info(const std::string& message) = 0;
    virtual void state_changed(bool paused) = 0;
    virtual void report(const ConsumerSnapshot& snapshot, const QueueStatus& queue, bool paused) = 0;
    virtual void defect(const ValidationResult& result, std::size_t bytes) = 0;
    virtual void summary(const ConsumerTotals& totals, std::chrono::nanoseconds elapsed) = 0;
};

class ConsoleConsumerReporter final : public IConsumerReporter {
public:
    explicit ConsoleConsumerReporter(std::ostream& out) noexcept : out_(out) {}
    void info(const std::string& message) override;
    void state_changed(bool paused) override;
    void report(const ConsumerSnapshot& snapshot, const QueueStatus& queue, bool paused) override;
    void defect(const ValidationResult& result, std::size_t bytes) override;
    void summary(const ConsumerTotals& totals, std::chrono::nanoseconds elapsed) override;

private:
    std::ostream& out_;
};

}  // namespace pc
