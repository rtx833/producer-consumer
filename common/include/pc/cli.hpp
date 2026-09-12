#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pc {

// Thrown for malformed command lines; the caller prints the message and usage.
class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Minimal declarative command-line parser: long/short options with values,
// boolean flags, positional arguments and generated --help text.
class CommandLine {
public:
    class Parsed {
    public:
        // Value of an option: explicit value, otherwise its default, otherwise nullopt.
        [[nodiscard]] std::optional<std::string> value(std::string_view name) const;
        // True when a flag was given or an option was set explicitly.
        [[nodiscard]] bool given(std::string_view name) const;
        [[nodiscard]] const std::vector<std::string>& positionals() const noexcept { return positionals_; }

    private:
        friend class CommandLine;
        struct Entry {
            std::string name;
            std::string value;
            bool explicit_value;
        };
        std::vector<Entry> entries_;
        std::vector<std::string> positionals_;
    };

    CommandLine(std::string program, std::string summary);

    CommandLine& option(std::string name, char short_name, std::string value_name,
                        std::string description, std::string default_value = {});
    CommandLine& flag(std::string name, char short_name, std::string description);
    CommandLine& positional(std::string name, std::string description);

    [[nodiscard]] Parsed parse(int argc, char* const* argv) const;
    [[nodiscard]] std::string help() const;

private:
    struct Spec {
        std::string name;
        char short_name;
        std::string value_name;  // empty for flags
        std::string description;
        std::string default_value;
        bool is_flag() const noexcept { return value_name.empty(); }
    };
    struct Positional {
        std::string name;
        std::string description;
    };

    const Spec* find_long(std::string_view name) const noexcept;
    const Spec* find_short(char name) const noexcept;

    std::string program_;
    std::string summary_;
    std::vector<Spec> specs_;
    std::vector<Positional> positionals_;
};

}  // namespace pc
