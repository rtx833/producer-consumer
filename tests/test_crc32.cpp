#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <vector>

#include "check.hpp"
#include "pc/checksum.hpp"

namespace {

// Straightforward bit-at-a-time reference implementation.
std::uint32_t reference_crc32(std::span<const std::uint8_t> data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace

int main() {
    const pc::Crc32 crc;

    const char* check_value = "123456789";
    const std::span<const std::uint8_t> known(reinterpret_cast<const std::uint8_t*>(check_value), 9);
    CHECK(crc.compute(known) == 0xCBF43926u);
    CHECK(crc.compute({}) == 0u);

    std::mt19937 rng(42);
    for (std::size_t length = 0; length < 300; ++length) {
        std::vector<std::uint8_t> data(length);
        for (auto& byte : data) {
            byte = static_cast<std::uint8_t>(rng());
        }
        CHECK(crc.compute(data) == reference_crc32(data));

        // Incremental hashing over an arbitrary split must match a single pass.
        const std::size_t split = length == 0 ? 0 : rng() % length;
        std::uint32_t state = crc.begin();
        state = crc.update(state, std::span<const std::uint8_t>(data).first(split));
        state = crc.update(state, std::span<const std::uint8_t>(data).subspan(split));
        CHECK(crc.finish(state) == reference_crc32(data));
    }
    return 0;
}
