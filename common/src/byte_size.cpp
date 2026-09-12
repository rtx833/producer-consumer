#include "pc/byte_size.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>

namespace pc {

std::optional<std::uint64_t> parse_uint(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint64_t> parse_byte_size(std::string_view text) {
    std::size_t digits = 0;
    while (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits]))) {
        ++digits;
    }
    if (digits == 0) {
        return std::nullopt;
    }
    const auto value = parse_uint(text.substr(0, digits));
    if (!value) {
        return std::nullopt;
    }

    std::string_view suffix = text.substr(digits);
    std::uint64_t multiplier = 1;
    if (!suffix.empty()) {
        switch (std::tolower(static_cast<unsigned char>(suffix[0]))) {
            case 'k': multiplier = std::uint64_t{1} << 10; break;
            case 'm': multiplier = std::uint64_t{1} << 20; break;
            case 'g': multiplier = std::uint64_t{1} << 30; break;
            case 't': multiplier = std::uint64_t{1} << 40; break;
            case 'b': multiplier = 1; break;
            default: return std::nullopt;
        }
        suffix.remove_prefix(1);
        if (multiplier != 1 && !suffix.empty()) {
            // Accept an optional "B" / "iB" after the unit letter.
            if (suffix.size() > 2) {
                return std::nullopt;
            }
            for (const char c : suffix) {
                const auto lc = std::tolower(static_cast<unsigned char>(c));
                if (lc != 'i' && lc != 'b') {
                    return std::nullopt;
                }
            }
        } else if (!suffix.empty()) {
            return std::nullopt;
        }
    }

    if (*value > std::numeric_limits<std::uint64_t>::max() / multiplier) {
        return std::nullopt;
    }
    return *value * multiplier;
}

std::string format_bytes(double bytes) {
    static constexpr const char* kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    std::size_t unit = 0;
    while (bytes >= 1024.0 && unit + 1 < std::size(kUnits)) {
        bytes /= 1024.0;
        ++unit;
    }
    if (unit == 0) {
        return std::format("{:.0f} B", bytes);
    }
    return std::format("{:.2f} {}", bytes, kUnits[unit]);
}

std::string format_count(std::uint64_t value) {
    std::string digits = std::to_string(value);
    std::string out;
    out.reserve(digits.size() + digits.size() / 3);
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i != 0 && (digits.size() - i) % 3 == 0) {
            out.push_back(',');
        }
        out.push_back(digits[i]);
    }
    return out;
}

std::string format_duration_ns(std::int64_t nanoseconds) {
    const double ns = static_cast<double>(nanoseconds);
    const double abs_ns = std::fabs(ns);
    if (abs_ns < 1'000.0) {
        return std::format("{:.0f} ns", ns);
    }
    if (abs_ns < 1'000'000.0) {
        return std::format("{:.1f} us", ns / 1e3);
    }
    if (abs_ns < 1'000'000'000.0) {
        return std::format("{:.1f} ms", ns / 1e6);
    }
    return std::format("{:.2f} s", ns / 1e9);
}

}  // namespace pc
