#ifndef HEXUTIL_HPP
#define HEXUTIL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Small text <-> bytes helpers with no GUI dependencies.
namespace hexutil {

// Parses hex bytes such as "DE AD be ef", "0xDE,0xAD" or "DEADBEEF".
// Tokens are separated by whitespace or commas; each token may start with 0x
// and must contain an even number of hex digits. Anything else is rejected
// (the old parser silently skipped stray characters).
bool parse_hex(const std::string& text, std::vector<std::uint8_t>& out,
               std::string* error = nullptr);

// "DE AD BE EF" (upper case, single spaces).
std::string to_hex(const std::vector<std::uint8_t>& bytes);

// Parses a file offset. "0x1F" and "0X1f" are hex; anything else is decimal.
// Surrounding whitespace is ignored.
bool parse_offset(const std::string& text, std::size_t& out);

// What the ASCII pane shows for a byte.
inline char printable(std::uint8_t b) { return (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.'; }

inline std::vector<std::uint8_t> from_text(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

}  // namespace hexutil

#endif
