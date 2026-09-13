#include "pause_policy.hpp"

#include <chrono>
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

}  // namespace pc
