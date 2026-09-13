#pragma once

#include <atomic>
#include <csignal>
#include <thread>

#include "pc/control.hpp"

namespace pc {

// Maps POSIX signals to RunControl operations:
//   SIGINT, SIGTERM, SIGHUP -> stop
//   SIGUSR1                 -> pause
//   SIGUSR2                 -> resume
// The signals are blocked in the constructing thread (and therefore in every
// thread created afterwards) and consumed synchronously by a dedicated thread
// using sigwait(). No asynchronous handlers run, so no async-signal-safety
// constraints apply. Construct it before any other thread.
class SignalControlSource final : public IControlSource {
public:
    explicit SignalControlSource(RunControl& control);
    ~SignalControlSource() override;

    SignalControlSource(const SignalControlSource&) = delete;
    SignalControlSource& operator=(const SignalControlSource&) = delete;

private:
    void run() noexcept;

    RunControl& control_;
    sigset_t signals_{};
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};

}  // namespace pc
