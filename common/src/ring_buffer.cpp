#include "pc/ring_buffer.hpp"

#include <cstring>
#include <limits>

namespace pc {

void initialize_ring_control(RingControl& control, std::uint64_t capacity, std::int32_t producer_pid) noexcept {
    control.magic = kRingMagic;
    control.version = kRingVersion;
    control.capacity = capacity;
    control.producer_pid.store(producer_pid, std::memory_order_relaxed);
    control.consumer_pid.store(0, std::memory_order_relaxed);
    control.head.store(0, std::memory_order_relaxed);
    control.tail.store(0, std::memory_order_relaxed);
    control.state.store(static_cast<std::uint32_t>(RingState::Ready), std::memory_order_release);
}

bool ring_control_ready(const RingControl& control) noexcept {
    // Acquire on `state` makes every field written before the Ready store visible.
    if (control.state.load(std::memory_order_acquire) != static_cast<std::uint32_t>(RingState::Ready)) {
        return false;
    }
    return control.magic == kRingMagic && control.version == kRingVersion && control.capacity != 0 &&
           (control.capacity & (control.capacity - 1)) == 0;
}

// ---------------------------------------------------------------- producer

RingProducer::RingProducer(RingControl& control, std::uint8_t* mirrored_data) noexcept
    : control_(control),
      data_(mirrored_data),
      capacity_(control.capacity),
      mask_(control.capacity - 1),
      head_(control.head.load(std::memory_order_relaxed)),
      cached_tail_(control.tail.load(std::memory_order_acquire)) {}

std::size_t RingProducer::max_record_size() const noexcept {
    const std::uint64_t by_capacity = capacity_ - kRecordPrefixSize;
    const std::uint64_t by_prefix = std::numeric_limits<RecordLength>::max();
    return static_cast<std::size_t>(by_capacity < by_prefix ? by_capacity : by_prefix);
}

std::optional<std::span<std::uint8_t>> RingProducer::try_reserve(std::size_t bytes) noexcept {
    if (bytes > max_record_size()) {
        return std::nullopt;
    }
    const std::uint64_t needed = kRecordPrefixSize + bytes;
    if (capacity_ - (head_ - cached_tail_) < needed) {
        cached_tail_ = control_.tail.load(std::memory_order_acquire);
        if (capacity_ - (head_ - cached_tail_) < needed) {
            return std::nullopt;
        }
    }
    const std::uint64_t offset = head_ & mask_;
    const auto length = static_cast<RecordLength>(bytes);
    std::memcpy(data_ + offset, &length, sizeof length);
    pending_ = needed;
    return std::span<std::uint8_t>(data_ + offset + kRecordPrefixSize, bytes);
}

void RingProducer::commit() noexcept {
    if (pending_ == 0) {
        return;
    }
    head_ += pending_;
    pending_ = 0;
    // Release: the record bytes and its length prefix become visible to a
    // consumer that acquires `head`.
    control_.head.store(head_, std::memory_order_release);
}

std::uint64_t RingProducer::used() const noexcept {
    return head_ - control_.tail.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------- consumer

RingConsumer::RingConsumer(RingControl& control, std::uint8_t* mirrored_data) noexcept
    : control_(control),
      data_(mirrored_data),
      capacity_(control.capacity),
      mask_(control.capacity - 1),
      tail_(control.tail.load(std::memory_order_relaxed)),
      cached_head_(control.head.load(std::memory_order_acquire)) {}

std::optional<std::span<const std::uint8_t>> RingConsumer::try_peek() noexcept {
    if (corrupted_) {
        return std::nullopt;
    }
    const std::uint64_t offset = tail_ & mask_;
    if (pending_ != 0) {
        return std::span<const std::uint8_t>(data_ + offset + kRecordPrefixSize, pending_ - kRecordPrefixSize);
    }
    if (cached_head_ == tail_) {
        cached_head_ = control_.head.load(std::memory_order_acquire);
        if (cached_head_ == tail_) {
            return std::nullopt;
        }
    }
    RecordLength length = 0;
    std::memcpy(&length, data_ + offset, sizeof length);
    const std::uint64_t record = kRecordPrefixSize + length;
    if (record > cached_head_ - tail_ || record > capacity_) {
        corrupted_ = true;
        return std::nullopt;
    }
    pending_ = record;
    return std::span<const std::uint8_t>(data_ + offset + kRecordPrefixSize, length);
}

void RingConsumer::release() noexcept {
    if (pending_ == 0) {
        return;
    }
    tail_ += pending_;
    pending_ = 0;
    // Release: our reads of the record happen-before the producer's reuse of
    // that space (the producer acquires `tail`).
    control_.tail.store(tail_, std::memory_order_release);
}

std::uint64_t RingConsumer::used() const noexcept {
    return control_.head.load(std::memory_order_acquire) - tail_;
}

}  // namespace pc
