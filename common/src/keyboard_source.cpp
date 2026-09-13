#include "pc/keyboard_source.hpp"

#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>

namespace pc {

RawTerminal::RawTerminal(int fd) : fd_(fd) {
    if (tcgetattr(fd_, &saved_) != 0) {
        throw std::system_error(errno, std::generic_category(), "tcgetattr");
    }
    termios raw = saved_;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(fd_, TCSANOW, &raw) != 0) {
        throw std::system_error(errno, std::generic_category(), "tcsetattr");
    }
}

RawTerminal::~RawTerminal() { tcsetattr(fd_, TCSANOW, &saved_); }

bool KeyboardControlSource::available(int fd) noexcept {
    return isatty(fd) == 1 && tcgetpgrp(fd) == getpgrp();
}

KeyboardControlSource::KeyboardControlSource(RunControl& control, int fd)
    : control_(control), terminal_(fd), thread_([this, fd](std::stop_token stop) { run(stop, fd); }) {}

void KeyboardControlSource::run(std::stop_token stop, int fd) noexcept {
    pollfd waiter{};
    waiter.fd = fd;
    waiter.events = POLLIN;
    while (!stop.stop_requested() && !control_.stop_requested()) {
        waiter.revents = 0;
        const int rc = poll(&waiter, 1, 100);
        if (rc <= 0) {
            continue;
        }
        if (waiter.revents & (POLLHUP | POLLERR | POLLNVAL)) {
            return;
        }
        unsigned char key = 0;
        const ssize_t n = read(fd, &key, 1);
        if (n == 0) {
            return;  // end of input
        }
        if (n < 0) {
            continue;
        }
        if (key == 'q' || key == 'Q') {
            control_.request_stop();
        } else {
            control_.toggle_pause();
        }
    }
}

}  // namespace pc
