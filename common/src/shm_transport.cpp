#include "pc/shm_transport.hpp"

#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <format>
#include <new>
#include <stdexcept>
#include <thread>

#include "pc/backoff.hpp"
#include "pc/byte_size.hpp"

namespace pc {
namespace {

bool process_alive(std::int32_t pid) noexcept {
    if (pid <= 0) {
        return false;
    }
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}

RingControl* control_of(MirroredShmRegion& region) noexcept {
    return std::launder(reinterpret_cast<RingControl*>(region.header()));
}

// Runs `attempt` until it succeeds, `stop` is requested, or `timeout` expires.
template <typename Attempt>
auto wait_until(Attempt&& attempt, std::chrono::milliseconds timeout, const std::stop_token& stop)
    -> decltype(attempt()) {
    using Clock = std::chrono::steady_clock;
    if (auto result = attempt()) {
        return result;
    }
    if (timeout.count() <= 0) {
        return std::nullopt;
    }
    const auto deadline = Clock::now() + timeout;
    Backoff backoff;
    for (;;) {
        if (stop.stop_requested()) {
            return std::nullopt;
        }
        backoff.wait();
        if (auto result = attempt()) {
            return result;
        }
        if (backoff.sleeping() && Clock::now() >= deadline) {
            return std::nullopt;
        }
    }
}

}  // namespace

// ---------------------------------------------------------------- sink

std::size_t ShmPacketSink::normalize_capacity(std::size_t requested) {
    std::size_t capacity = MirroredShmRegion::page_size();
    while (capacity < requested) {
        if (capacity > (std::size_t{1} << 62)) {
            throw std::invalid_argument("ring capacity is too large");
        }
        capacity <<= 1;
    }
    return capacity;
}

ShmPacketSink::ShmPacketSink(std::string name, std::size_t capacity)
    : region_([&] {
          std::optional<MirroredShmRegion> existing;
          try {
              existing = MirroredShmRegion::try_open(name);
          } catch (const std::exception&) {
              // Unreadable or malformed leftover: create() replaces it below.
          }
          if (existing) {
              const RingControl* control = control_of(*existing);
              if (ring_control_ready(*control)) {
                  const auto pid = control->producer_pid.load(std::memory_order_relaxed);
                  if (process_alive(pid) && pid != getpid()) {
                      throw std::runtime_error(
                          std::format("another producer (pid {}) is already using {}", pid, name));
                  }
              }
          }
          return MirroredShmRegion::create(name, normalize_capacity(capacity));
      }()),
      control_([&] {
          auto* control = new (region_.header()) RingControl{};
          initialize_ring_control(*control, region_.data_capacity(), static_cast<std::int32_t>(getpid()));
          return control;
      }()),
      ring_(*control_, region_.data()) {}

ShmPacketSink::~ShmPacketSink() {
    control_->state.store(static_cast<std::uint32_t>(RingState::ProducerFinished), std::memory_order_release);
    region_.unlink();
}

std::optional<std::span<std::uint8_t>> ShmPacketSink::reserve(std::size_t bytes, std::chrono::milliseconds timeout,
                                                              std::stop_token stop) {
    return wait_until([&] { return ring_.try_reserve(bytes); }, timeout, stop);
}

void ShmPacketSink::commit() { ring_.commit(); }

bool ShmPacketSink::wait_for_peer(std::stop_token stop) {
    while (!stop.stop_requested()) {
        if (peer_status().alive) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

std::size_t ShmPacketSink::max_packet_size() const { return ring_.max_record_size(); }

QueueStatus ShmPacketSink::queue_status() const { return {ring_.used(), ring_.capacity()}; }

PeerStatus ShmPacketSink::peer_status() const {
    const auto pid = control_->consumer_pid.load(std::memory_order_acquire);
    return {pid != 0, process_alive(pid), false};
}

std::string ShmPacketSink::describe() const {
    return std::format("shm:{} ({} ring)", region_.name(), format_bytes(static_cast<double>(ring_.capacity())));
}

// ---------------------------------------------------------------- source

ShmPacketSource::ShmPacketSource(std::string name, std::chrono::milliseconds poll_interval)
    : name_(std::move(name)), poll_interval_(poll_interval) {}

ShmPacketSource::~ShmPacketSource() { disconnect(); }

bool ShmPacketSource::try_attach() {
    auto region = MirroredShmRegion::try_open(name_);
    if (!region) {
        return false;
    }
    RingControl* control = control_of(*region);
    if (!ring_control_ready(*control)) {
        return false;
    }
    if (control->capacity != region->data_capacity()) {
        throw std::runtime_error("shared memory ring capacity does not match the mapped size");
    }
    if (!process_alive(control->producer_pid.load(std::memory_order_relaxed))) {
        return false;  // leftover of a crashed producer; the next producer recreates it
    }
    const auto me = static_cast<std::int32_t>(getpid());
    std::int32_t expected = control->consumer_pid.load(std::memory_order_acquire);
    if (expected != 0 && expected != me && process_alive(expected)) {
        throw std::runtime_error(std::format("another consumer (pid {}) is already attached to {}", expected, name_));
    }
    if (!control->consumer_pid.compare_exchange_strong(expected, me, std::memory_order_acq_rel)) {
        return false;  // lost a race with another consumer; report it on the next attempt
    }
    region_ = std::move(region);
    control_ = control;
    ring_.emplace(*control_, region_->data());
    return true;
}

bool ShmPacketSource::connect(std::stop_token stop) {
    if (connected()) {
        return true;
    }
    while (!stop.stop_requested()) {
        if (try_attach()) {
            return true;
        }
        std::this_thread::sleep_for(poll_interval_);
    }
    return false;
}

void ShmPacketSource::disconnect() {
    if (!connected()) {
        return;
    }
    ring_->release();
    auto me = static_cast<std::int32_t>(getpid());
    control_->consumer_pid.compare_exchange_strong(me, 0, std::memory_order_acq_rel);
    ring_.reset();
    control_ = nullptr;
    region_.reset();
}

std::optional<std::span<const std::uint8_t>> ShmPacketSource::receive(std::chrono::milliseconds timeout,
                                                                      std::stop_token stop) {
    if (!connected()) {
        return std::nullopt;
    }
    ring_->release();
    return wait_until([&] { return ring_->try_peek(); }, timeout, stop);
}

void ShmPacketSource::release() {
    if (connected()) {
        ring_->release();
    }
}

QueueStatus ShmPacketSource::queue_status() const {
    if (!connected()) {
        return {};
    }
    return {ring_->used(), ring_->capacity()};
}

PeerStatus ShmPacketSource::peer_status() const {
    if (!connected()) {
        return {};
    }
    const auto pid = control_->producer_pid.load(std::memory_order_relaxed);
    const auto state = control_->state.load(std::memory_order_acquire);
    const bool finished = state == static_cast<std::uint32_t>(RingState::ProducerFinished) || ring_->corrupted();
    return {pid != 0, process_alive(pid), finished};
}

std::string ShmPacketSource::describe() const {
    if (!connected()) {
        return std::format("shm:{}", name_);
    }
    return std::format("shm:{} ({} ring, producer pid {})", name_,
                       format_bytes(static_cast<double>(ring_->capacity())),
                       control_->producer_pid.load(std::memory_order_relaxed));
}

}  // namespace pc
