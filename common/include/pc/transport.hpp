#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stop_token>
#include <string>

namespace pc {

struct QueueStatus {
    std::uint64_t used_bytes = 0;
    std::uint64_t capacity_bytes = 0;

    [[nodiscard]] double fill_ratio() const noexcept {
        return capacity_bytes == 0 ? 0.0 : static_cast<double>(used_bytes) / static_cast<double>(capacity_bytes);
    }
};

struct PeerStatus {
    bool present = false;   // a peer has attached at some point
    bool alive = false;     // the peer process still exists
    bool finished = false;  // the peer announced an orderly end of stream
};

// Producer-side transport. Packets are written in place: the caller reserves
// a slot, fills it, and commits it (zero-copy on the producer side).
class IPacketSink {
public:
    virtual ~IPacketSink() = default;

    // Waits up to `timeout` for room for one packet of `bytes` bytes. Returns
    // nullopt on timeout or when `stop` is requested.
    [[nodiscard]] virtual std::optional<std::span<std::uint8_t>> reserve(std::size_t bytes,
                                                                         std::chrono::milliseconds timeout,
                                                                         std::stop_token stop) = 0;
    // Publishes the last reserved packet.
    virtual void commit() = 0;

    // Blocks until a consumer attaches or `stop` is requested; returns false in the latter case.
    virtual bool wait_for_peer(std::stop_token stop) = 0;

    [[nodiscard]] virtual std::size_t max_packet_size() const = 0;
    [[nodiscard]] virtual QueueStatus queue_status() const = 0;
    [[nodiscard]] virtual PeerStatus peer_status() const = 0;
    [[nodiscard]] virtual std::string describe() const = 0;
};

// Consumer-side transport. Packets are read in place: the returned view
// stays valid until release() or the next receive() call.
class IPacketSource {
public:
    virtual ~IPacketSource() = default;

    // Blocks until a producer is available or `stop` is requested; returns
    // false in the latter case.
    virtual bool connect(std::stop_token stop) = 0;
    virtual void disconnect() = 0;
    [[nodiscard]] virtual bool connected() const = 0;

    // Waits up to `timeout` for the next packet. Any packet returned by the
    // previous call is released first.
    [[nodiscard]] virtual std::optional<std::span<const std::uint8_t>> receive(std::chrono::milliseconds timeout,
                                                                               std::stop_token stop) = 0;
    // Releases the packet returned by the last receive(), if any.
    virtual void release() = 0;

    [[nodiscard]] virtual QueueStatus queue_status() const = 0;
    [[nodiscard]] virtual PeerStatus peer_status() const = 0;
    [[nodiscard]] virtual std::string describe() const = 0;
};

}  // namespace pc
