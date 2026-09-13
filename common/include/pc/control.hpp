#pragma once

#include <atomic>
#include <stop_token>

namespace pc {

// Shared run state of an application: a cooperative stop request and a
// pause flag. Written by control sources (signals, keyboard), read by the
// main processing loop. All operations are thread-safe.
class RunControl {
public:
    void request_stop() noexcept { stop_source_.request_stop(); }
    [[nodiscard]] bool stop_requested() const noexcept { return stop_source_.stop_requested(); }
    [[nodiscard]] std::stop_token stop_token() const noexcept { return stop_source_.get_token(); }

    void pause() noexcept { paused_.store(1, std::memory_order_relaxed); }
    void resume() noexcept { paused_.store(0, std::memory_order_relaxed); }
    void toggle_pause() noexcept { paused_.fetch_xor(1, std::memory_order_relaxed); }
    [[nodiscard]] bool paused() const noexcept { return paused_.load(std::memory_order_relaxed) != 0; }

private:
    std::stop_source stop_source_;
    std::atomic<unsigned> paused_{0};
};

// A source of control events (signals, keyboard, ...). Implementations own
// whatever thread or handler they need and drive a RunControl. Applications
// only keep them alive; they never need to know the concrete type.
class IControlSource {
public:
    virtual ~IControlSource() = default;
};

}  // namespace pc
