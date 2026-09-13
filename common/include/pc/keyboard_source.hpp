#pragma once

#include <termios.h>

#include <thread>

#include "pc/control.hpp"

namespace pc {

// Puts a terminal into non-canonical, no-echo mode for its lifetime so that
// single key presses are delivered without waiting for Enter. Signal
// generation (Ctrl-C) stays enabled.
class RawTerminal {
public:
    explicit RawTerminal(int fd);
    ~RawTerminal();

    RawTerminal(const RawTerminal&) = delete;
    RawTerminal& operator=(const RawTerminal&) = delete;

private:
    int fd_;
    termios saved_{};
};

// Any key toggles pause/resume; 'q' requests a stop. Only usable when the
// descriptor is an interactive terminal owned by this process' foreground
// process group (see available()).
class KeyboardControlSource final : public IControlSource {
public:
    static bool available(int fd) noexcept;

    explicit KeyboardControlSource(RunControl& control, int fd = 0);
    ~KeyboardControlSource() override = default;

    KeyboardControlSource(const KeyboardControlSource&) = delete;
    KeyboardControlSource& operator=(const KeyboardControlSource&) = delete;

private:
    void run(std::stop_token stop, int fd) noexcept;

    RunControl& control_;
    RawTerminal terminal_;
    std::jthread thread_;
};

}  // namespace pc
