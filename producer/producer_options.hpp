#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>

namespace pc {

struct ProducerOptions {
    std::size_t payload_size = 0;
    std::string shm_name;
    std::size_t ring_size = 0;
    std::string generator;
    std::uint64_t seed = 0;
    double rate = 0.0;
    std::uint64_t count = 0;
    std::chrono::milliseconds report_interval{1000};
    bool keyboard = true;
    bool wait_for_consumer = false;
};

// Parses the command line. Prints help to `out` and returns nullopt for
// --help; throws UsageError for invalid input.
std::optional<ProducerOptions> parse_producer_options(int argc, char* const* argv, std::ostream& out);

}  // namespace pc
