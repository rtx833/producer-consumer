#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace pc {

// Produces the payload bytes of a packet directly into the destination
// buffer (which is the transport's shared memory slot: no intermediate copy).
class IPayloadGenerator {
public:
    virtual ~IPayloadGenerator() = default;
    virtual void fill(std::span<std::uint8_t> payload) noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

// Pseudo-random bytes from a SplitMix64 generator (fast, 8 bytes per step).
class RandomPayloadGenerator final : public IPayloadGenerator {
public:
    explicit RandomPayloadGenerator(std::uint64_t seed) noexcept : state_(seed) {}
    void fill(std::span<std::uint8_t> payload) noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override { return "random"; }

private:
    std::uint64_t next() noexcept;
    std::uint64_t state_;
};

// A byte counter that keeps running across packets: 0,1,2,...,255,0,1,...
class SequentialPayloadGenerator final : public IPayloadGenerator {
public:
    void fill(std::span<std::uint8_t> payload) noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override { return "sequential"; }

private:
    std::uint8_t next_ = 0;
};

// Factory used by the composition root; throws std::invalid_argument for
// unknown kinds. Registered kinds: "random", "sequential".
std::unique_ptr<IPayloadGenerator> make_payload_generator(std::string_view kind, std::uint64_t seed);

}  // namespace pc
