#include <array>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "payload_generator.hpp"

int main() {
    // Sequential: a byte counter that continues across calls and sizes.
    {
        pc::SequentialPayloadGenerator gen;
        std::vector<std::uint8_t> a(300), b(7);
        gen.fill(a);
        gen.fill(b);
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK(a[i] == static_cast<std::uint8_t>(i));
        }
        for (std::size_t i = 0; i < b.size(); ++i) {
            CHECK(b[i] == static_cast<std::uint8_t>(300 + i));
        }
    }

    // Random: deterministic per seed, different across seeds, fills exactly
    // the requested bytes for sizes that are not multiples of the block.
    {
        pc::RandomPayloadGenerator g1(42), g2(42), g3(43);
        std::vector<std::uint8_t> a(1000), b(1000), c(1000);
        g1.fill(a);
        g2.fill(b);
        g3.fill(c);
        CHECK(a == b);
        CHECK(a != c);

        for (const std::size_t size : {0u, 1u, 7u, 63u, 64u, 65u, 1000u}) {
            std::vector<std::uint8_t> buf(size + 16, 0xEE);
            pc::RandomPayloadGenerator g(7);
            g.fill(std::span<std::uint8_t>(buf.data(), size));
            for (std::size_t i = size; i < buf.size(); ++i) {
                CHECK(buf[i] == 0xEE);  // nothing written past the requested span
            }
        }

        // Every byte value shows up and none dominates in a 64 KiB sample.
        pc::RandomPayloadGenerator g(1);
        std::vector<std::uint8_t> sample(64 * 1024);
        g.fill(sample);
        std::array<std::size_t, 256> histogram{};
        for (const auto byte : sample) {
            ++histogram[byte];
        }
        for (const auto count : histogram) {
            CHECK(count > 128 && count < 384);  // expected 256 per value
        }

        // Consecutive calls continue the stream rather than restarting it.
        pc::RandomPayloadGenerator g4(42), g5(42);
        std::vector<std::uint8_t> whole(256), first(128), second(128);
        g4.fill(whole);
        g5.fill(first);
        g5.fill(second);
        CHECK(std::vector<std::uint8_t>(whole.begin(), whole.begin() + 128) == first);
        CHECK(std::vector<std::uint8_t>(whole.begin() + 128, whole.end()) == second);
    }
    return 0;
}
