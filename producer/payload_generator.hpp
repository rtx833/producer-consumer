#pragma once

#include <cstddef>
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

// Pseudo-random bytes from eight independent xoshiro256+ generators stepped
// in lock-step. The state is laid out lane-wise (structure of arrays), so the
// per-lane loop vectorises: every step produces 64 bytes with a handful of
// SIMD shifts, xors and adds and no multiplications. On x86 an AVX2 build of
// the same loop is selected at run time when the CPU supports it.
class RandomPayloadGenerator final : public IPayloadGenerator {
public:
    static constexpr std::size_t kLanes = 8;

    explicit RandomPayloadGenerator(std::uint64_t seed) noexcept;
    void fill(std::span<std::uint8_t> payload) noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;

    struct State {
        std::uint64_t s[4][kLanes];
    };

private:
    State state_;
    bool avx2_;
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
