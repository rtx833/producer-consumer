#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <new>
#include <random>
#include <vector>

#include "check.hpp"
#include "pc/ring_buffer.hpp"
#include "pc/shm_region.hpp"

int main() {
    const std::string name = std::format("/pc-test-ring-{}", getpid());
    const std::size_t capacity = pc::MirroredShmRegion::page_size();

    {
        auto region = pc::MirroredShmRegion::create(name, capacity);
        region.unlink();  // the mapping stays alive for the rest of the test

        // Mirrored mapping: a byte written at offset i is visible at i + capacity.
        region.data()[0] = 0xAB;
        region.data()[capacity - 1] = 0xCD;
        CHECK(region.data()[capacity] == 0xAB);
        CHECK(region.data()[2 * capacity - 1] == 0xCD);

        auto* control = new (region.header()) pc::RingControl{};
        pc::initialize_ring_control(*control, capacity, static_cast<std::int32_t>(getpid()));
        CHECK(pc::ring_control_ready(*control));

        pc::RingProducer producer(*control, region.data());
        pc::RingConsumer consumer(*control, region.data());
        CHECK(!consumer.try_peek().has_value());
        CHECK(producer.max_record_size() == capacity - pc::kRecordPrefixSize);

        // A record larger than the ring is rejected outright.
        CHECK(!producer.try_reserve(capacity).has_value());

        // Fill the ring completely, then verify the full condition and drain it.
        const std::size_t record = 100;
        std::size_t written = 0;
        while (auto slot = producer.try_reserve(record)) {
            std::memset(slot->data(), static_cast<int>(written & 0xFF), slot->size());
            producer.commit();
            ++written;
        }
        CHECK(written == capacity / (record + pc::kRecordPrefixSize));
        CHECK(producer.used() == written * (record + pc::kRecordPrefixSize));
        for (std::size_t i = 0; i < written; ++i) {
            const auto view = consumer.try_peek();
            CHECK(view.has_value());
            CHECK(view->size() == record);
            CHECK((*view)[0] == static_cast<std::uint8_t>(i & 0xFF));
            CHECK((*view)[record - 1] == static_cast<std::uint8_t>(i & 0xFF));
            consumer.release();
        }
        CHECK(!consumer.try_peek().has_value());
        CHECK(consumer.used() == 0);

        // Many records of random sizes so that the head wraps around repeatedly;
        // every record must come back intact and in order.
        std::mt19937 rng(7);
        std::uint64_t sent = 0;
        std::uint64_t received = 0;
        std::vector<std::uint8_t> expected;
        for (int round = 0; round < 20000; ++round) {
            const std::size_t size = 1 + rng() % 700;
            const auto slot = producer.try_reserve(size);
            if (!slot) {
                // Full: consume one record and try again.
                const auto view = consumer.try_peek();
                CHECK(view.has_value());
                expected.assign(view->size(), static_cast<std::uint8_t>(received & 0xFF));
                CHECK(std::memcmp(view->data(), expected.data(), view->size()) == 0);
                consumer.release();
                ++received;
                continue;
            }
            CHECK(slot->size() == size);
            std::memset(slot->data(), static_cast<int>(sent & 0xFF), size);
            producer.commit();
            ++sent;
        }
        while (const auto view = consumer.try_peek()) {
            expected.assign(view->size(), static_cast<std::uint8_t>(received & 0xFF));
            CHECK(std::memcmp(view->data(), expected.data(), view->size()) == 0);
            consumer.release();
            ++received;
        }
        CHECK(sent == received);
        CHECK(control->head.load() == control->tail.load());
        CHECK(!consumer.corrupted());
    }

    // The name was unlinked: a fresh open must not find it.
    CHECK(!pc::MirroredShmRegion::try_open(name).has_value());
    return 0;
}
