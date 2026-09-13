#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "pc/ring_buffer.hpp"
#include "pc/shm_region.hpp"
#include "pc/transport.hpp"

namespace pc {

inline constexpr const char* kDefaultShmName = "/pc-ring";

// IPacketSink over a shared-memory ring. The producer owns the shared memory
// object: it creates it on construction, marks the stream finished and unlinks
// the name on destruction (a still-attached consumer keeps its mapping and can
// drain the remaining packets).
class ShmPacketSink final : public IPacketSink {
public:
    // `capacity` is rounded up to a power of two that is at least one page.
    ShmPacketSink(std::string name, std::size_t capacity);
    ~ShmPacketSink() override;

    std::optional<std::span<std::uint8_t>> reserve(std::size_t bytes, std::chrono::milliseconds timeout,
                                                   std::stop_token stop) override;
    void commit() override;
    bool wait_for_peer(std::stop_token stop) override;

    std::size_t max_packet_size() const override;
    QueueStatus queue_status() const override;
    PeerStatus peer_status() const override;
    std::string describe() const override;

    static std::size_t normalize_capacity(std::size_t requested);

private:
    MirroredShmRegion region_;
    RingControl* control_;
    RingProducer ring_;
};

// IPacketSource over a shared-memory ring created by ShmPacketSink.
class ShmPacketSource final : public IPacketSource {
public:
    explicit ShmPacketSource(std::string name,
                             std::chrono::milliseconds poll_interval = std::chrono::milliseconds(100));
    ~ShmPacketSource() override;

    bool connect(std::stop_token stop) override;
    void disconnect() override;
    bool connected() const override { return ring_.has_value(); }

    std::optional<std::span<const std::uint8_t>> receive(std::chrono::milliseconds timeout,
                                                         std::stop_token stop) override;
    void release() override;

    QueueStatus queue_status() const override;
    PeerStatus peer_status() const override;
    std::string describe() const override;

private:
    bool try_attach();

    std::string name_;
    std::chrono::milliseconds poll_interval_;
    std::optional<MirroredShmRegion> region_;
    RingControl* control_ = nullptr;
    std::optional<RingConsumer> ring_;
};

}  // namespace pc
