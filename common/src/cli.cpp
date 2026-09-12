#include "pc/cli.hpp"

#include <algorithm>
#include <format>

namespace pc {

std::optional<std::string> CommandLine::Parsed::value(std::string_view name) const {
    for (const auto& entry : entries_) {
        if (entry.name == name) {
            return entry.value;
        }
    }
    return std::nullopt;
}

bool CommandLine::Parsed::given(std::string_view name) const {
    return std::any_of(entries_.begin(), entries_.end(),
                       [&](const Entry& e) { return e.name == name && e.explicit_value; });
}

CommandLine::CommandLine(std::string program, std::string summary)
    : program_(std::move(program)), summary_(std::move(summary)) {
    flag("help", 'h', "Show this help and exit");
}

CommandLine& CommandLine::option(std::string name, char short_name, std::string value_name,
                                 std::string description, std::string default_value) {
    specs_.push_back(Spec{std::move(name), short_name, std::move(value_name), std::move(description),
                          std::move(default_value)});
    return *this;
}

CommandLine& CommandLine::flag(std::string name, char short_name, std::string description) {
    specs_.push_back(Spec{std::move(name), short_name, {}, std::move(description), {}});
    return *this;
}

CommandLine& CommandLine::positional(std::string name, std::string description) {
    positionals_.push_back(Positional{std::move(name), std::move(description)});
    return *this;
}

const CommandLine::Spec* CommandLine::find_long(std::string_view name) const noexcept {
    for (const auto& spec : specs_) {
        if (spec.name == name) {
            return &spec;
        }
    }
    return nullptr;
}

const CommandLine::Spec* CommandLine::find_short(char name) const noexcept {
    for (const auto& spec : specs_) {
        if (spec.short_name != '\0' && spec.short_name == name) {
            return &spec;
        }
    }
    return nullptr;
}

CommandLine::Parsed CommandLine::parse(int argc, char* const* argv) const {
    Parsed parsed;
    for (const auto& spec : specs_) {
        if (!spec.is_flag()) {
            parsed.entries_.push_back({spec.name, spec.default_value, false});
        }
    }
    auto assign = [&](const Spec& spec, std::string value) {
        for (auto& entry : parsed.entries_) {
            if (entry.name == spec.name) {
                entry.value = std::move(value);
                entry.explicit_value = true;
                return;
            }
        }
        parsed.entries_.push_back({spec.name, std::move(value), true});
    };

    bool only_positionals = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (only_positionals || arg.size() < 2 || arg[0] != '-') {
            parsed.positionals_.emplace_back(arg);
            continue;
        }
        if (arg == "--") {
            only_positionals = true;
            continue;
        }

        const Spec* spec = nullptr;
        std::optional<std::string> inline_value;
        if (arg.starts_with("--")) {
            std::string_view name = arg.substr(2);
            if (const auto eq = name.find('='); eq != std::string_view::npos) {
                inline_value = std::string(name.substr(eq + 1));
                name = name.substr(0, eq);
            }
            spec = find_long(name);
            if (spec == nullptr) {
                throw UsageError(std::format("unknown option '--{}'", name));
            }
        } else {
            spec = find_short(arg[1]);
            if (spec == nullptr) {
                throw UsageError(std::format("unknown option '-{}'", arg[1]));
            }
            if (arg.size() > 2) {
                inline_value = std::string(arg.substr(2));
            }
        }

        if (spec->is_flag()) {
            if (inline_value) {
                throw UsageError(std::format("option '--{}' does not take a value", spec->name));
            }
            assign(*spec, "true");
            continue;
        }
        if (inline_value) {
            assign(*spec, std::move(*inline_value));
        } else if (i + 1 < argc) {
            assign(*spec, argv[++i]);
        } else {
            throw UsageError(std::format("option '--{}' requires a value", spec->name));
        }
    }

    if (parsed.positionals_.size() > positionals_.size()) {
        throw UsageError(std::format("unexpected argument '{}'", parsed.positionals_[positionals_.size()]));
    }
    return parsed;
}

std::string CommandLine::help() const {
    std::string text = std::format("Usage: {} [options]", program_);
    for (const auto& pos : positionals_) {
        text += std::format(" [{}]", pos.name);
    }
    text += "\n\n" + summary_ + "\n";
    if (!positionals_.empty()) {
        text += "\nArguments:\n";
        for (const auto& pos : positionals_) {
            text += std::format("  {:<26} {}\n", pos.name, pos.description);
        }
    }
    text += "\nOptions:\n";
    for (const auto& spec : specs_) {
        std::string left = spec.short_name != '\0' ? std::format("-{}, --{}", spec.short_name, spec.name)
                                                   : std::format("    --{}", spec.name);
        if (!spec.is_flag()) {
            left += " <" + spec.value_name + ">";
        }
        std::string right = spec.description;
        if (!spec.default_value.empty()) {
            right += std::format(" (default: {})", spec.default_value);
        }
        text += std::format("  {:<26} {}\n", left, right);
    }
    return text;
}

}  // namespace pc
