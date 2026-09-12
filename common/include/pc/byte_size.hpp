#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pc {

// Parses a byte size such as "4096", "64K", "16M", "1G" (binary multiples;
// suffix is case-insensitive and may be followed by "B" or "iB").
std::optional<std::uint64_t> parse_byte_size(std::string_view text);

// Parses an unsigned decimal integer, rejecting anything else.
std::optional<std::uint64_t> parse_uint(std::string_view text);

// Human readable byte count: "512 B", "1.50 MiB", "2.01 GiB".
std::string format_bytes(double bytes);

// Decimal integer with thousands separators: "1,234,567".
std::string format_count(std::uint64_t value);

// Duration given in nanoseconds: "850 ns", "3.2 us", "12.5 ms", "1.20 s".
std::string format_duration_ns(std::int64_t nanoseconds);

}  // namespace pc
