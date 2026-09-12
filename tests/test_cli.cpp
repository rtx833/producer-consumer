#include <string>

#include "check.hpp"
#include "pc/byte_size.hpp"
#include "pc/cli.hpp"

int main() {
    // Byte sizes.
    CHECK(pc::parse_byte_size("4096") == 4096u);
    CHECK(pc::parse_byte_size("4K") == 4096u);
    CHECK(pc::parse_byte_size("4k") == 4096u);
    CHECK(pc::parse_byte_size("4KiB") == 4096u);
    CHECK(pc::parse_byte_size("64M") == 64u * 1024 * 1024);
    CHECK(pc::parse_byte_size("1GB") == 1024u * 1024 * 1024);
    CHECK(pc::parse_byte_size("0") == 0u);
    CHECK(!pc::parse_byte_size("").has_value());
    CHECK(!pc::parse_byte_size("K").has_value());
    CHECK(!pc::parse_byte_size("12X").has_value());
    CHECK(!pc::parse_byte_size("-1").has_value());
    CHECK(!pc::parse_byte_size("99999999999999999999").has_value());
    CHECK(pc::parse_uint("42") == 42u);
    CHECK(!pc::parse_uint("4 2").has_value());
    CHECK(!pc::parse_uint("1.5").has_value());

    CHECK(pc::format_bytes(512) == "512 B");
    CHECK(pc::format_bytes(1536) == "1.50 KiB");
    CHECK(pc::format_count(1234567) == "1,234,567");
    CHECK(pc::format_count(999) == "999");
    CHECK(pc::format_count(19881) == "19,881");
    CHECK(pc::format_count(1000) == "1,000");
    CHECK(pc::format_count(0) == "0");
    CHECK(pc::format_duration_ns(850) == "850 ns");
    CHECK(pc::format_duration_ns(3'200) == "3.2 us");

    // Command line parsing.
    pc::CommandLine cli("tool", "summary");
    cli.positional("size", "payload size")
        .option("name", 'n', "str", "a name", "default")
        .option("count", 'c', "n", "a count", "0")
        .flag("verbose", 'v', "be chatty");

    {
        const char* argv[] = {"tool", "1024", "-n", "abc", "--count=7", "-v"};
        const auto parsed = cli.parse(6, const_cast<char* const*>(argv));
        CHECK(parsed.positionals().size() == 1 && parsed.positionals()[0] == "1024");
        CHECK(parsed.value("name") == "abc");
        CHECK(parsed.value("count") == "7");
        CHECK(parsed.given("verbose"));
        CHECK(!parsed.given("help"));
    }
    {
        const char* argv[] = {"tool"};
        const auto parsed = cli.parse(1, const_cast<char* const*>(argv));
        CHECK(parsed.value("name") == "default");
        CHECK(!parsed.given("name"));
        CHECK(!parsed.given("verbose"));
        CHECK(parsed.positionals().empty());
    }
    {
        const char* argv[] = {"tool", "-c9", "--", "-literal"};
        const auto parsed = cli.parse(4, const_cast<char* const*>(argv));
        CHECK(parsed.value("count") == "9");
        CHECK(parsed.positionals().size() == 1 && parsed.positionals()[0] == "-literal");
    }
    for (const char* bad : {"--bogus", "-x", "--count", "--verbose=1"}) {
        const char* argv[] = {"tool", bad};
        bool threw = false;
        try {
            (void)cli.parse(2, const_cast<char* const*>(argv));
        } catch (const pc::UsageError&) {
            threw = true;
        }
        CHECK(threw);
    }
    {
        const char* argv[] = {"tool", "1", "2"};
        bool threw = false;
        try {
            (void)cli.parse(3, const_cast<char* const*>(argv));
        } catch (const pc::UsageError&) {
            threw = true;
        }
        CHECK(threw);
    }
    CHECK(cli.help().find("--count <n>") != std::string::npos);
    return 0;
}
