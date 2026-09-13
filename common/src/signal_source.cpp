#include "pc/signal_source.hpp"

#include <pthread.h>

#include <cerrno>
#include <system_error>

namespace pc {

SignalControlSource::SignalControlSource(RunControl& control) : control_(control) {
    sigemptyset(&signals_);
    for (const int sig : {SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2}) {
        sigaddset(&signals_, sig);
    }
    if (const int rc = pthread_sigmask(SIG_BLOCK, &signals_, nullptr); rc != 0) {
        throw std::system_error(rc, std::generic_category(), "pthread_sigmask");
    }
    thread_ = std::thread([this] { run(); });
}

SignalControlSource::~SignalControlSource() {
    stopping_.store(true);
    // Wake the waiting thread with one of the signals it listens for; it
    // checks `stopping_` before acting on anything.
    pthread_kill(thread_.native_handle(), SIGUSR2);
    thread_.join();
}

void SignalControlSource::run() noexcept {
    for (;;) {
        int sig = 0;
        if (sigwait(&signals_, &sig) != 0) {
            continue;
        }
        if (stopping_.load()) {
            return;
        }
        switch (sig) {
            case SIGINT:
            case SIGTERM:
            case SIGHUP:
                control_.request_stop();
                break;
            case SIGUSR1:
                control_.pause();
                break;
            case SIGUSR2:
                control_.resume();
                break;
            default:
                break;
        }
    }
}

}  // namespace pc
