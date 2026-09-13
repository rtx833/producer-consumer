#include "payload_generator.hpp"

#include <cstring>
#include <format>
#include <stdexcept>

namespace pc {

std::uint64_t RandomPayloadGenerator::next() noexcept {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void RandomPayloadGenerator::fill(std::span<std::uint8_t> payload) noexcept {
    std::uint8_t* out = payload.data();
    std::size_t remaining = payload.size();
    while (remaining >= sizeof(std::uint64_t)) {
        const std::uint64_t value = next();
        std::memcpy(out, &value, sizeof value);
        out += sizeof value;
        remaining -= sizeof value;
    }
    if (remaining != 0) {
        const std::uint64_t value = next();
        std::memcpy(out, &value, remaining);
    }
}

void SequentialPayloadGenerator::fill(std::span<std::uint8_t> payload) noexcept {
    for (std::uint8_t& byte : payload) {
        byte = next_++;
    }
}

std::unique_ptr<IPayloadGenerator> make_payload_generator(std::string_view kind, std::uint64_t seed) {
    if (kind == "random") {
        return std::make_unique<RandomPayloadGenerator>(seed);
    }
    if (kind == "sequential") {
        return std::make_unique<SequentialPayloadGenerator>();
    }
    throw std::invalid_argument(std::format("unknown payload generator '{}' (expected: random, sequential)", kind));
}

}  // namespace pc
