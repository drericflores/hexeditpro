#include "HexUtil.hpp"

#include <cctype>
#include <limits>

namespace hexutil {

namespace {

int hex_value(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void set_error(std::string* error, const std::string& message) {
    if (error) *error = message;
}

}  // namespace

bool parse_hex(const std::string& text, std::vector<std::uint8_t>& out, std::string* error) {
    out.clear();

    std::size_t i = 0;
    const std::size_t n = text.size();
    while (i < n) {
        while (i < n && (std::isspace(static_cast<unsigned char>(text[i])) || text[i] == ',')) ++i;
        if (i >= n) break;

        std::size_t j = i;
        while (j < n && !std::isspace(static_cast<unsigned char>(text[j])) && text[j] != ',') ++j;
        std::string token = text.substr(i, j - i);
        i = j;

        std::string digits = token;
        if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
            digits.erase(0, 2);
        }
        if (digits.empty()) {
            set_error(error, "'" + token + "' has no hex digits");
            out.clear();
            return false;
        }
        for (unsigned char c : digits) {
            if (hex_value(c) < 0) {
                set_error(error, "'" + token + "' is not a hex value");
                out.clear();
                return false;
            }
        }
        if (digits.size() % 2 != 0) {
            set_error(error, "'" + token + "' has an odd number of hex digits");
            out.clear();
            return false;
        }
        for (std::size_t k = 0; k < digits.size(); k += 2) {
            out.push_back(static_cast<std::uint8_t>(
                (hex_value(static_cast<unsigned char>(digits[k])) << 4) |
                hex_value(static_cast<unsigned char>(digits[k + 1]))));
        }
    }

    if (out.empty()) {
        set_error(error, "No bytes entered");
        return false;
    }
    return true;
}

std::string to_hex(const std::vector<std::uint8_t>& bytes) {
    static const char* digits = "0123456789ABCDEF";
    std::string s;
    s.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i) s.push_back(' ');
        s.push_back(digits[bytes[i] >> 4]);
        s.push_back(digits[bytes[i] & 0x0F]);
    }
    return s;
}

bool parse_offset(const std::string& text, std::size_t& out) {
    std::size_t b = 0, e = text.size();
    while (b < e && std::isspace(static_cast<unsigned char>(text[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(text[e - 1]))) --e;
    if (b == e) return false;

    unsigned base = 10;
    if (e - b >= 2 && text[b] == '0' && (text[b + 1] == 'x' || text[b + 1] == 'X')) {
        base = 16;
        b += 2;
        if (b == e) return false;
    }

    std::size_t value = 0;
    constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = b; i < e; ++i) {
        const int d = hex_value(static_cast<unsigned char>(text[i]));
        if (d < 0 || static_cast<unsigned>(d) >= base) return false;
        if (value > (max - static_cast<std::size_t>(d)) / base) return false;  // overflow
        value = value * base + static_cast<std::size_t>(d);
    }
    out = value;
    return true;
}

}  // namespace hexutil
