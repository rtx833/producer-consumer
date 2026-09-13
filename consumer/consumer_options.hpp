#pragma once

#include <chrono>
#include <optional>
#include <ostream>
#include <string>

namespace pc {

struct ConsumerOptions {
    std::string shm_name;
    std::string pause_policy;
    std::chrono::milliseconds report_interval{1000};
    unsigned max_defect_logs = 10;
    bool keyboard = true;
    bool once = false;
};

// Parses the command line. Prints help to `out` and returns nullopt for
// --help; throws UsageError for invalid input.
std::optional<ConsumerOptions> parse_consumer_options(int argc, char* const* argv, std::ostream& out);

}  // namespace pc
