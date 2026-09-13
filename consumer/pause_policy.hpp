#pragma once

#include <memory>
#include <stop_token>
#include <string_view>

#include "consumer_stats.hpp"
#include "packet_validator.hpp"
#include "pc/transport.hpp"

namespace pc {

// Decides what happens to incoming packets while the consumer is paused.
class IPausePolicy {
public:
    virtual ~IPausePolicy() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::string_view description() const noexcept = 0;
    // Invoked repeatedly while paused; must return within a few milliseconds.
    virtual void while_paused(IPacketSource& source, ConsumerStats& stats, std::stop_token stop) = 0;
    // Invoked once when the consumer resumes.
    virtual void on_resume(IPacketValidator& validator) = 0;
};

// Stops reading. Packets accumulate in the transport queue; once it is full
// the producer blocks (back-pressure). Nothing is lost; everything is
// delivered after resume.
class BlockingPausePolicy final : public IPausePolicy {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "block"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "packets queue up in the ring; the producer blocks when it is full; nothing is lost";
    }
    void while_paused(IPacketSource& source, ConsumerStats& stats, std::stop_token stop) override;
    void on_resume(IPacketValidator& validator) override;
};

}  // namespace pc
