#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pc {

inline constexpr std::uint32_t kRingMagic = 0x42524350u;  // "PCRB"
inline constexpr std::uint32_t kRingVersion = 1;
inline constexpr std::size_t kCacheLineSize = 64;

enum class RingState : std::uint32_t {
    Initializing = 0,
    Ready = 1,
    ProducerFinished = 2,
};

// Control block placed at the start of the shared memory region. The head and
// tail counters grow monotonically (they are byte offsets into an infinite
// stream); positions inside the data area are `counter & (capacity - 1)`.
// Each counter lives on its own cache line: the producer only writes `head`,
// the consumer only writes `tail`, so no line ping-pongs on writes.
struct RingControl {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t capacity;  // power of two
    std::atomic<std::uint32_t> state;
    std::atomic<std::int32_t> producer_pid;
    std::atomic<std::int32_t> consumer_pid;
    alignas(kCacheLineSize) std::atomic<std::uint64_t> head;  // bytes published by the producer
    alignas(kCacheLineSize) std::atomic<std::uint64_t> tail;  // bytes released by the consumer
};

static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<std::int32_t>::is_always_lock_free);
static_assert(sizeof(RingControl) <= 4096, "control block must fit into one page");

using RecordLength = std::uint32_t;
inline constexpr std::size_t kRecordPrefixSize = sizeof(RecordLength);

// Initializes a zeroed control block; the last store publishes it as Ready.
void initialize_ring_control(RingControl& control, std::uint64_t capacity, std::int32_t producer_pid) noexcept;
[[nodiscard]] bool ring_control_ready(const RingControl& control) noexcept;

// Single-producer end of a wait-free byte-record ring: records are
// [uint32 length][bytes]. Expects `data` to be a mirrored mapping (see
// MirroredShmRegion), so every record is contiguous in virtual memory.
class RingProducer {
public:
    RingProducer(RingControl& control, std::uint8_t* mirrored_data) noexcept;

    // Reserves room for one record. Returns a writable view into the ring or
    // nullopt when the free space is insufficient (never blocks).
    [[nodiscard]] std::optional<std::span<std::uint8_t>> try_reserve(std::size_t bytes) noexcept;
    // Publishes the record reserved by the last successful try_reserve().
    void commit() noexcept;

    [[nodiscard]] std::uint64_t used() const noexcept;
    [[nodiscard]] std::uint64_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t max_record_size() const noexcept;

private:
    RingControl& control_;
    std::uint8_t* data_;
    std::uint64_t capacity_;
    std::uint64_t mask_;
    std::uint64_t head_;         // private copy of control_.head (the producer is its only writer)
    std::uint64_t cached_tail_;  // last observed control_.tail; refreshed only when space runs out
    std::uint64_t pending_ = 0;  // size of the reserved-but-uncommitted record, including prefix
};

// Single-consumer end of the ring.
class RingConsumer {
public:
    RingConsumer(RingControl& control, std::uint8_t* mirrored_data) noexcept;

    // Returns the next record without consuming it, or nullopt when the ring
    // is empty. Calling it again before release() returns the same record.
    [[nodiscard]] std::optional<std::span<const std::uint8_t>> try_peek() noexcept;
    // Frees the record returned by try_peek() so the producer can reuse the space.
    void release() noexcept;

    [[nodiscard]] std::uint64_t used() const noexcept;
    [[nodiscard]] std::uint64_t capacity() const noexcept { return capacity_; }
    // Set when the ring content violates the record format (foreign or
    // damaged control block); the consumer should abandon the ring.
    [[nodiscard]] bool corrupted() const noexcept { return corrupted_; }

private:
    RingControl& control_;
    std::uint8_t* data_;
    std::uint64_t capacity_;
    std::uint64_t mask_;
    std::uint64_t tail_;         // private copy of control_.tail (the consumer is its only writer)
    std::uint64_t cached_head_;  // last observed control_.head; refreshed only when the ring looks empty
    std::uint64_t pending_ = 0;  // size of the peeked-but-unreleased record, including prefix
    bool corrupted_ = false;
};

}  // namespace pc
