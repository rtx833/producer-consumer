#include "consumer_options.hpp"

#include <format>

#include "pc/byte_size.hpp"
#include "pc/cli.hpp"
#include "pc/shm_transport.hpp"

namespace pc {
namespace {

std::uint64_t uint_option(const CommandLine::Parsed& parsed, std::string_view name) {
    const auto text = parsed.value(name).value_or("");
    const auto value = parse_uint(text);
    if (!value) {
        throw UsageError(std::format("--{}: '{}' is not a non-negative integer", name, text));
    }
    return *value;
}

}  // namespace

std::optional<ConsumerOptions> parse_consumer_options(int argc, char* const* argv, std::ostream& out) {
    CommandLine cli("consumer",
                    "Receives packets from the producer through a shared-memory ring buffer, verifies\n"
                    "their metadata and checksum and prints statistics once per report interval.\n"
                    "Any key toggles pause; 'q' or Ctrl-C quits.\n"
                    "Signals: SIGUSR1 pause, SIGUSR2 resume, SIGINT/SIGTERM stop.");
    cli.option("shm-name", 'n', "name", "POSIX shared memory object name", kDefaultShmName)
        .option("pause-policy", 'p', "policy",
                "What happens while paused: block (producer blocks, nothing lost) | discard (drop packets)",
                "block")
        .option("report-interval", 0, "ms", "Statistics report period in milliseconds", "1000")
        .option("log-defects", 0, "n", "Print details for at most n defective packets per session", "10")
        .flag("once", '1', "Exit when the producer finishes instead of waiting for the next one")
        .flag("no-keyboard", 0, "Do not read keys from the terminal");

    const auto parsed = cli.parse(argc, argv);
    if (parsed.given("help")) {
        out << cli.help();
        return std::nullopt;
    }

    ConsumerOptions options;
    options.shm_name = parsed.value("shm-name").value_or(kDefaultShmName);
    options.pause_policy = parsed.value("pause-policy").value_or("block");
    const auto interval = uint_option(parsed, "report-interval");
    if (interval == 0) {
        throw UsageError("--report-interval must be positive");
    }
    options.report_interval = std::chrono::milliseconds(interval);
    options.max_defect_logs = static_cast<unsigned>(uint_option(parsed, "log-defects"));
    options.keyboard = !parsed.given("no-keyboard");
    options.once = parsed.given("once");
    return options;
}

}  // namespace pc
