#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <vector>

#include "check.hpp"
#include "pc/checksum.hpp"

namespace {

// Straightforward bit-at-a-time reference implementation.
std::uint32_t reference_crc(std::uint32_t polynomial, std::span<const std::uint8_t> data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1u) ? (polynomial ^ (crc >> 1)) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void check_against_reference(const pc::IChecksum& crc, std::uint32_t polynomial) {
    std::mt19937 rng(42);
    for (std::size_t length = 0; length < 300; ++length) {
        std::vector<std::uint8_t> data(length);
        for (auto& byte : data) {
            byte = static_cast<std::uint8_t>(rng());
        }
        const std::uint32_t expected = reference_crc(polynomial, data);
        CHECK(crc.compute(data) == expected);

        // Incremental hashing over an arbitrary split must match a single pass.
        const std::size_t split = length == 0 ? 0 : rng() % length;
        std::uint32_t state = crc.begin();
        state = crc.update(state, std::span<const std::uint8_t>(data).first(split));
        state = crc.update(state, std::span<const std::uint8_t>(data).subspan(split));
        CHECK(crc.finish(state) == expected);
    }
}

}  // namespace

int main() {
    const char* check_value = "123456789";
    const std::span<const std::uint8_t> known(reinterpret_cast<const std::uint8_t*>(check_value), 9);

    // CRC-32 (IEEE): standard check value 0xCBF43926.
    const pc::Crc32 crc32;
    CHECK(crc32.kind() == pc::ChecksumKind::Crc32);
    CHECK(crc32.compute(known) == 0xCBF43926u);
    CHECK(crc32.compute({}) == 0u);
    check_against_reference(crc32, 0xEDB88320u);

    // CRC-32C (Castagnoli): standard check value 0xE3069283, for both the
    // software table and, where the CPU has it, the hardware instruction.
    const pc::Crc32c crc32c_software(/*allow_hardware=*/false);
    CHECK(!crc32c_software.hardware());
    CHECK(crc32c_software.kind() == pc::ChecksumKind::Crc32c);
    CHECK(crc32c_software.compute(known) == 0xE3069283u);
    check_against_reference(crc32c_software, 0x82F63B78u);

    const pc::Crc32c crc32c_default;
    CHECK(crc32c_default.hardware() == pc::Crc32c::hardware_available());
    CHECK(crc32c_default.compute(known) == 0xE3069283u);
    check_against_reference(crc32c_default, 0x82F63B78u);

    // Registry and factory agree with the classes.
    const pc::ChecksumRegistry registry;
    CHECK(registry.find(pc::ChecksumKind::Crc32)->compute(known) == 0xCBF43926u);
    CHECK(registry.find(pc::ChecksumKind::Crc32c)->compute(known) == 0xE3069283u);
    CHECK(registry.find(static_cast<pc::ChecksumKind>(200)) == nullptr);
    CHECK(pc::make_checksum("crc32")->kind() == pc::ChecksumKind::Crc32);
    CHECK(pc::make_checksum("crc32c")->kind() == pc::ChecksumKind::Crc32c);
    bool threw = false;
    try {
        (void)pc::make_checksum("md5");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    return 0;
}
