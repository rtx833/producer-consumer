#include "pause_policy.hpp"

#include <chrono>
#include <format>
#include <stdexcept>
#include <thread>

namespace pc {

namespace {
constexpr auto kPausedTick = std::chrono::milliseconds(10);
}

void BlockingPausePolicy::while_paused(IPacketSource& source, ConsumerStats& /*stats*/, std::stop_token /*stop*/) {
    source.release();  // hand back the last processed slot; then simply wait
    std::this_thread::sleep_for(kPausedTick);
}

void BlockingPausePolicy::on_resume(IPacketValidator& /*validator*/) {}

void DiscardingPausePolicy::while_paused(IPacketSource& source, ConsumerStats& stats, std::stop_token stop) {
    if (const auto packet = source.receive(kPausedTick, stop)) {
        stats.record_dropped(packet->size());
    }
}

void DiscardingPausePolicy::on_resume(IPacketValidator& validator) { validator.resync(); }

std::unique_ptr<IPausePolicy> make_pause_policy(std::string_view name) {
    if (name == "block") {
        return std::make_unique<BlockingPausePolicy>();
    }
    if (name == "discard") {
        return std::make_unique<DiscardingPausePolicy>();
    }
    throw std::invalid_argument(std::format("unknown pause policy '{}' (expected: block, discard)", name));
}

}  // namespace pc
