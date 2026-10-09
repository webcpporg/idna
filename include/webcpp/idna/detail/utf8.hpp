// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Strict UTF-8 (RFC 3629): decoding one code point, encoding one, and checking a
 whole string.
*/
#ifndef WEBCPP_IDNA_DETAIL_UTF8_HPP
#define WEBCPP_IDNA_DETAIL_UTF8_HPP

#include <webcpp/idna/config.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace webcpp::idna::detail::utf8 {

/**
 Decodes the code point that starts at `text[i]` into `cp` and moves `i` past it, or returns false
 and leaves both unchanged at an ill-formed sequence: truncated, overlong, a surrogate, above
 U+10FFFF, or a byte no sequence starts with.
*/
constexpr bool decode(std::string_view text, std::size_t& i, char32_t& cp) noexcept {
    if (i >= text.size()) {
        return false;
    }
    const auto byte = [&text](std::size_t at) { return static_cast<unsigned char>(text[at]); };
    const unsigned char lead = byte(i);
    if (lead < 0x80) {
        cp = lead;
        ++i;
        return true;
    }
    std::size_t count = 0;
    char32_t value = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        count = 1;
        value = lead & 0x1FU;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        count = 2;
        value = lead & 0x0FU;
        low = lead == 0xE0 ? 0xA0 : low;
        high = lead == 0xED ? 0x9F : high;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        count = 3;
        value = lead & 0x07U;
        low = lead == 0xF0 ? 0x90 : low;
        high = lead == 0xF4 ? 0x8F : high;
    } else {
        return false;
    }
    if (text.size() - i <= count) {
        return false;
    }
    for (std::size_t k = 1; k <= count; ++k) {
        const unsigned char next = byte(i + k);
        if (next < low || next > high) {
            return false;
        }
        low = 0x80;
        high = 0xBF;
        value = value << 6U | (next & 0x3FU);
    }
    cp = value;
    i += count + 1;
    return true;
}

/** Whether every byte of `text` belongs to a well-formed UTF-8 sequence. */
constexpr bool is_valid(std::string_view text) noexcept {
    std::size_t i = 0;
    char32_t cp = 0;
    while (i < text.size()) {
        if (!decode(text, i, cp)) {
            return false;
        }
    }
    return true;
}

/** The number of octets of the UTF-8 of the scalar value `cp`. */
constexpr std::size_t length(char32_t cp) noexcept {
    if (cp < 0x80) {
        return 1;
    }
    if (cp < 0x800) {
        return 2;
    }
    return cp < 0x10000 ? 3 : 4;
}

/** Writes the UTF-8 of the scalar value `cp` at `out`, which holds @ref length(cp) octets, and
    returns that length. */
constexpr std::size_t encode(char32_t cp, char* out) noexcept {
    const std::size_t size = length(cp);
    if (size == 1) {
        out[0] = static_cast<char>(cp);
        return 1;
    }
    constexpr std::array<unsigned char, 5> leads{0, 0, 0xC0, 0xE0, 0xF0};
    out[0] = static_cast<char>(leads[size] | (cp >> (6 * (size - 1))));
    for (std::size_t k = 1; k < size; ++k) {
        out[k] = static_cast<char>(0x80U | ((cp >> (6 * (size - 1 - k))) & 0x3FU));
    }
    return size;
}

}  // namespace webcpp::idna::detail::utf8

#endif  // WEBCPP_IDNA_DETAIL_UTF8_HPP
