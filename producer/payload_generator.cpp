#include "payload_generator.hpp"

#include <cstring>
#include <format>
#include <stdexcept>

namespace pc {

namespace {

constexpr std::size_t kLanes = RandomPayloadGenerator::kLanes;
constexpr std::size_t kBlockBytes = kLanes * sizeof(std::uint64_t);  // 64 bytes per step

std::uint64_t splitmix64(std::uint64_t& state) noexcept {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Explicit SIMD via the GCC/Clang vector extension: one v4u64 holds four
// lanes, two of them cover the eight generators. The compiler lowers the
// element-wise operators to whatever the target has (AVX2 ymm, SSE2 xmm
// pairs, NEON), so vectorisation does not depend on auto-vectoriser
// heuristics. Locals keep the state in registers for the whole fill.
using v4u64 = std::uint64_t __attribute__((vector_size(32)));
constexpr std::size_t kVectors = kLanes / 4;

struct Lanes {
    v4u64 s0[kVectors], s1[kVectors], s2[kVectors], s3[kVectors];
};

inline __attribute__((always_inline)) Lanes load_lanes(const RandomPayloadGenerator::State& st) noexcept {
    Lanes v;
    std::memcpy(v.s0, st.s[0], sizeof v.s0);
    std::memcpy(v.s1, st.s[1], sizeof v.s1);
    std::memcpy(v.s2, st.s[2], sizeof v.s2);
    std::memcpy(v.s3, st.s[3], sizeof v.s3);
    return v;
}

inline __attribute__((always_inline)) void store_lanes(RandomPayloadGenerator::State& st, const Lanes& v) noexcept {
    std::memcpy(st.s[0], v.s0, sizeof v.s0);
    std::memcpy(st.s[1], v.s1, sizeof v.s1);
    std::memcpy(st.s[2], v.s2, sizeof v.s2);
    std::memcpy(st.s[3], v.s3, sizeof v.s3);
}

// One xoshiro256+ step for all lanes; writes 64 output bytes.
inline __attribute__((always_inline)) void xoshiro_step(Lanes& v, std::uint8_t* out) noexcept {
    for (std::size_t j = 0; j < kVectors; ++j) {
        const v4u64 result = v.s0[j] + v.s3[j];
        const v4u64 t = v.s1[j] << 17;
        v.s2[j] ^= v.s0[j];
        v.s3[j] ^= v.s1[j];
        v.s1[j] ^= v.s2[j];
        v.s0[j] ^= v.s3[j];
        v.s2[j] ^= t;
        v.s3[j] = (v.s3[j] << 45) | (v.s3[j] >> 19);
        std::memcpy(out + j * sizeof(v4u64), &result, sizeof result);
    }
}

inline __attribute__((always_inline)) void fill_impl(RandomPayloadGenerator::State& state,
                                                     std::span<std::uint8_t> payload) noexcept {
    Lanes v = load_lanes(state);
    std::uint8_t* out = payload.data();
    std::size_t remaining = payload.size();
    while (remaining >= kBlockBytes) {
        xoshiro_step(v, out);
        out += kBlockBytes;
        remaining -= kBlockBytes;
    }
    if (remaining != 0) {
        std::uint8_t block[kBlockBytes];
        xoshiro_step(v, block);
        std::memcpy(out, block, remaining);
    }
    store_lanes(state, v);
}

void fill_portable(RandomPayloadGenerator::State& st, std::span<std::uint8_t> payload) noexcept {
    fill_impl(st, payload);
}

#if defined(__x86_64__) || defined(__i386__)
#define PC_HAVE_AVX2_CLONE 1
__attribute__((target("avx2"))) void fill_avx2(RandomPayloadGenerator::State& st,
                                               std::span<std::uint8_t> payload) noexcept {
    fill_impl(st, payload);
}
bool avx2_available() noexcept { return __builtin_cpu_supports("avx2"); }
#else
bool avx2_available() noexcept { return false; }
#endif

}  // namespace

RandomPayloadGenerator::RandomPayloadGenerator(std::uint64_t seed) noexcept : state_{}, avx2_(avx2_available()) {
    // Seed every lane from SplitMix64, as the xoshiro authors recommend; the
    // outputs of a SplitMix64 sequence are never all zero, so no lane can
    // start in xoshiro's single fixed point.
    std::uint64_t sm = seed;
    for (std::size_t l = 0; l < kLanes; ++l) {
        for (auto& word : state_.s) {
            word[l] = splitmix64(sm);
        }
    }
}

std::string_view RandomPayloadGenerator::name() const noexcept {
    return avx2_ ? "random (xoshiro256+ x8, AVX2)" : "random (xoshiro256+ x8)";
}

void RandomPayloadGenerator::fill(std::span<std::uint8_t> payload) noexcept {
#if defined(PC_HAVE_AVX2_CLONE)
    if (avx2_) {
        fill_avx2(state_, payload);
        return;
    }
#endif
    fill_portable(state_, payload);
}

void SequentialPayloadGenerator::fill(std::span<std::uint8_t> payload) noexcept {
    // Count in a local: with the member itself the compiler must assume the
    // destination may alias `this` and cannot vectorise the loop.
    std::uint8_t value = next_;
    for (std::uint8_t& byte : payload) {
        byte = value++;
    }
    next_ = value;
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
