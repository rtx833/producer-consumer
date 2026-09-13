#include "producer_options.hpp"

#include <charconv>
#include <format>

#include "pc/byte_size.hpp"
#include "pc/cli.hpp"
#include "pc/shm_transport.hpp"

namespace pc {
namespace {

std::uint64_t byte_size_option(const CommandLine::Parsed& parsed, std::string_view name) {
    const auto text = parsed.value(name).value_or("");
    const auto value = parse_byte_size(text);
    if (!value) {
        throw UsageError(std::format("--{}: '{}' is not a byte size (e.g. 4096, 64K, 16M)", name, text));
    }
    return *value;
}

std::uint64_t uint_option(const CommandLine::Parsed& parsed, std::string_view name) {
    const auto text = parsed.value(name).value_or("");
    const auto value = parse_uint(text);
    if (!value) {
        throw UsageError(std::format("--{}: '{}' is not a non-negative integer", name, text));
    }
    return *value;
}

double double_option(const CommandLine::Parsed& parsed, std::string_view name) {
    const auto text = parsed.value(name).value_or("");
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size() || value < 0.0) {
        throw UsageError(std::format("--{}: '{}' is not a non-negative number", name, text));
    }
    return value;
}

}  // namespace

std::optional<ProducerOptions> parse_producer_options(int argc, char* const* argv, std::ostream& out) {
    CommandLine cli("producer",
                    "Generates packets (header + payload) and streams them to the consumer through\n"
                    "a shared-memory ring buffer. Any key toggles pause; 'q' or Ctrl-C quits.\n"
                    "Signals: SIGUSR1 pause, SIGUSR2 resume, SIGINT/SIGTERM stop.");
    cli.positional("payload-size", "Payload size in bytes (same as --payload-size)")
        .option("payload-size", 's', "bytes", "Payload size per packet, e.g. 1024 or 4K")
        .option("shm-name", 'n', "name", "POSIX shared memory object name", kDefaultShmName)
        .option("ring-size", 'r', "bytes", "Ring buffer capacity (rounded up to a power of two)", "64M")
        .option("generator", 'g', "kind", "Payload generator: random | sequential", "random")
        .option("seed", 0, "n", "Seed for the random generator", "1")
        .option("rate", 0, "pps", "Packets per second, 0 = as fast as possible", "0")
        .option("count", 'c', "n", "Stop after this many packets, 0 = unlimited", "0")
        .option("report-interval", 0, "ms", "Statistics report period in milliseconds", "1000")
        .flag("wait-for-consumer", 'w', "Do not start until a consumer attaches")
        .flag("no-keyboard", 0, "Do not read keys from the terminal");

    const auto parsed = cli.parse(argc, argv);
    if (parsed.given("help")) {
        out << cli.help();
        return std::nullopt;
    }

    ProducerOptions options;
    std::optional<std::string> payload_text = parsed.value("payload-size");
    if (!parsed.positionals().empty()) {
        if (parsed.given("payload-size")) {
            throw UsageError("payload size given both as an argument and as --payload-size");
        }
        payload_text = parsed.positionals().front();
    }
    if (!payload_text) {
        throw UsageError("payload size is required (e.g. 'producer 1024' or 'producer --payload-size 4K')");
    }
    const auto payload = parse_byte_size(*payload_text);
    if (!payload) {
        throw UsageError(std::format("'{}' is not a byte size (e.g. 4096, 64K, 16M)", *payload_text));
    }
    options.payload_size = static_cast<std::size_t>(*payload);
    options.shm_name = parsed.value("shm-name").value_or(kDefaultShmName);
    options.ring_size = static_cast<std::size_t>(byte_size_option(parsed, "ring-size"));
    options.generator = parsed.value("generator").value_or("random");
    options.seed = uint_option(parsed, "seed");
    options.rate = double_option(parsed, "rate");
    options.count = uint_option(parsed, "count");
    const auto interval = uint_option(parsed, "report-interval");
    if (interval == 0) {
        throw UsageError("--report-interval must be positive");
    }
    options.report_interval = std::chrono::milliseconds(interval);
    options.keyboard = !parsed.given("no-keyboard");
    options.wait_for_consumer = parsed.given("wait-for-consumer");
    return options;
}

}  // namespace pc
