// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 Punycode (RFC 3492): code points to the ASCII of an A-label without its "xn--"
 prefix, and back, with errors as values.

 @see "Punycode", in the guide.
*/
#ifndef WEBCPP_IDNA_PUNYCODE_HPP
#define WEBCPP_IDNA_PUNYCODE_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/errors.hpp>

#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

namespace webcpp::idna {

namespace detail::punycode {

/** RFC 3492 section 5's parameters, and the 32-bit unsigned arithmetic of section 6.4. */
inline constexpr std::uint32_t base = 36;
/** RFC 3492's tmin. */
inline constexpr std::uint32_t tmin = 1;
/** RFC 3492's tmax. */
inline constexpr std::uint32_t tmax = 26;
/** RFC 3492's skew. */
inline constexpr std::uint32_t skew = 38;
/** RFC 3492's damp. */
inline constexpr std::uint32_t damp = 700;
/** RFC 3492's initial_bias. */
inline constexpr std::uint32_t initial_bias = 72;
/** RFC 3492's initial_n, the first code point that is not basic. */
inline constexpr std::uint32_t initial_n = 0x80;
/** The largest value of the 32-bit arithmetic. */
inline constexpr std::uint32_t max_value = std::numeric_limits<std::uint32_t>::max();

/** RFC 3492 section 6.1: the bias after a delta. */
constexpr std::uint32_t adapt(std::uint32_t delta, std::uint32_t points, bool first) noexcept {
    delta = first ? delta / damp : delta / 2;
    delta += delta / points;
    std::uint32_t k = 0;
    while (delta > (base - tmin) * tmax / 2) {
        delta /= base - tmin;
        k += base;
    }
    return k + ((base - tmin + 1) * delta / (delta + skew));
}

/** The threshold of the digit at position `k` under `bias`. */
constexpr std::uint32_t threshold(std::uint32_t k, std::uint32_t bias) noexcept {
    if (k <= bias) {
        return tmin;
    }
    return k >= bias + tmax ? tmax : k - bias;
}

/** The value of a digit, a to z (either case) then 0 to 9, or base for any other character. */
constexpr std::uint32_t digit_value(char c) noexcept {
    if (c >= 'a' && c <= 'z') {
        return static_cast<std::uint32_t>(c - 'a');
    }
    if (c >= 'A' && c <= 'Z') {
        return static_cast<std::uint32_t>(c - 'A');
    }
    if (c >= '0' && c <= '9') {
        return static_cast<std::uint32_t>(c - '0') + 26;
    }
    return base;
}

/** The lowercase character of a digit value below base. */
constexpr char digit_char(std::uint32_t d) noexcept {
    return d < 26 ? static_cast<char>('a' + d) : static_cast<char>('0' + (d - 26));
}

/** Whether `cp` is a Unicode scalar value: at most U+10FFFF, and no surrogate. */
constexpr bool is_scalar(char32_t cp) noexcept {
    return cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF);
}

/** Hands the variable-length integer `q` (RFC 3492 section 3.3) to `put` under `bias`; false
    when `put` refuses a digit. */
template <class Put>
constexpr bool put_integer(std::uint32_t q, std::uint32_t bias, Put& put) {
    for (std::uint32_t k = base;; k += base) {
        const std::uint32_t t = threshold(k, bias);
        if (q < t) {
            break;
        }
        if (!put(digit_char(t + ((q - t) % (base - t))))) {
            return false;
        }
        q = (q - t) / (base - t);
    }
    return put(digit_char(q));
}

/** The smallest code point of `input` that is at least `n`. */
constexpr char32_t smallest_from(std::u32string_view input, std::uint32_t n) noexcept {
    char32_t found = 0x10FFFF;
    for (const char32_t cp : input) {
        if (cp >= n && cp < found) {
            found = cp;
        }
    }
    return found;
}

/** Hands the basic code points of `input` to `put`, then the delimiter when there was any: their
    number, or output_too_small. */
template <class Put>
constexpr result<std::uint32_t> put_basic(std::u32string_view input, Put& put) {
    std::uint32_t basic = 0;
    for (const char32_t cp : input) {
        if (cp >= initial_n) {
            continue;
        }
        if (!put(static_cast<char>(cp))) {
            return error::output_too_small;
        }
        ++basic;
    }
    if (basic > 0 && !put('-')) {
        return error::output_too_small;
    }
    return basic;
}

/**
 RFC 3492 section 6.3 over `input`, handing each output character to `put`, which returns false
 when it cannot take it: the number of characters handed, or the first error.

 Tip: every code point is checked before the first character is handed, so a code point that is
 no scalar value is invalid_punycode whatever `put` can take.
*/
template <class Put>
constexpr result<std::size_t> encode(std::u32string_view input, Put put) {
    if (input.size() >= max_value) {
        return error::punycode_overflow;
    }
    if (!std::ranges::all_of(input, is_scalar)) {
        return error::invalid_punycode;
    }
    std::size_t written = 0;
    auto counted = [&](char c) {
        if (!put(c)) {
            return false;
        }
        ++written;
        return true;
    };
    const result<std::uint32_t> copied = put_basic(input, counted);
    if (copied.has_error()) {
        return copied.error();
    }
    const std::uint32_t basic = copied.unsafe_value();
    std::uint32_t n = initial_n;
    std::uint32_t delta = 0;
    std::uint32_t bias = initial_bias;
    for (std::uint32_t handled = basic; handled < input.size(); ++delta, ++n) {
        const char32_t next = smallest_from(input, n);
        if (next - n > (max_value - delta) / (handled + 1)) {
            return error::punycode_overflow;
        }
        delta += (next - n) * (handled + 1);
        n = next;
        for (const char32_t cp : input) {
            if (cp < n && ++delta == 0) {
                return error::punycode_overflow;
            }
            if (cp != n) {
                continue;
            }
            if (!put_integer(delta, bias, counted)) {
                return error::output_too_small;
            }
            bias = adapt(delta, handled + 1, handled == basic);
            delta = 0;
            ++handled;
        }
    }
    return written;
}

/**
 Reads the variable-length integer that starts at `input[in]` under `bias` and adds it, times its
 weights, to `i` (RFC 3492 section 6.2's inner loop): no error once a digit ends it.
*/
inline boost::system::error_code read_integer(std::string_view input, std::size_t& in,
                                              std::uint32_t& i, std::uint32_t bias) noexcept {
    std::uint32_t w = 1;
    for (std::uint32_t k = base;; k += base) {
        if (in >= input.size()) {
            return error::invalid_punycode;
        }
        const std::uint32_t digit = digit_value(input[in++]);
        if (digit >= base) {
            return error::invalid_punycode;
        }
        if (digit > (max_value - i) / w) {
            return error::punycode_overflow;
        }
        i += digit * w;
        const std::uint32_t t = threshold(k, bias);
        if (digit < t) {
            return {};
        }
        if (w > max_value / (base - t)) {
            return error::punycode_overflow;
        }
        w *= base - t;
    }
}

/**
 RFC 3492 section 6.2 over `input` into `output`: the number of code points written, or the first
 error.

 Tip: the basic code points are checked before any is written, so a byte above 0x7F before the
 delimiter is invalid_punycode whatever the size of `output`. The input's size is bounded as the
 encoder's is, so that the number of code points written plus one never wraps to zero.
*/
inline result<std::size_t> decode(std::span<char32_t> output, std::string_view input) noexcept {
    if (input.size() >= max_value) {
        return error::punycode_overflow;
    }
    const std::size_t delimiter = input.rfind('-');
    const std::size_t basic = delimiter == std::string_view::npos ? 0 : delimiter;
    const auto is_basic = [](char c) { return static_cast<unsigned char>(c) < initial_n; };
    if (!std::all_of(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(basic), is_basic)) {
        return error::invalid_punycode;
    }
    if (basic > output.size()) {
        return error::output_too_small;
    }
    for (std::size_t j = 0; j < basic; ++j) {
        output[j] = static_cast<unsigned char>(input[j]);
    }
    std::size_t written = basic;
    std::uint32_t n = initial_n;
    std::uint32_t i = 0;
    std::uint32_t bias = initial_bias;
    for (std::size_t in = basic > 0 ? basic + 1 : 0; in < input.size(); ++i) {
        const std::uint32_t old_i = i;
        if (const boost::system::error_code failed = read_integer(input, in, i, bias)) {
            return failed;
        }
        const auto points = static_cast<std::uint32_t>(written + 1);
        bias = adapt(i - old_i, points, old_i == 0);
        if (i / points > max_value - n) {
            return error::punycode_overflow;
        }
        n += i / points;
        i %= points;
        if (n < initial_n || !is_scalar(n)) {
            return error::invalid_punycode;
        }
        if (written == output.size()) {
            return error::output_too_small;
        }
        std::copy_backward(output.begin() + static_cast<std::ptrdiff_t>(i),
                           output.begin() + static_cast<std::ptrdiff_t>(written),
                           output.begin() + static_cast<std::ptrdiff_t>(written) + 1);
        output[i] = n;
        ++written;
    }
    return written;
}

}  // namespace detail::punycode

/**
 Punycode (RFC 3492), the encoding of an A-label's code points after its "xn--"
 prefix, which UTS 46 and IDNA2008 apply to one label at a time.

 @see "Punycode", in the guide.
*/
namespace punycode {

/**
 Encodes code points as Punycode, into a caller's buffer, without the "xn--"
 prefix (RFC 3492 section 6.3).

 Basic code points, below U+0080, are copied as they are, case included, and
 then a delimiter `-` when there is any; the digits that follow are lowercase.
 The encoding stops when `output` is full, so its time grows at most with
 the input's length times the size of `output`.

 @param output Where the characters are written; nothing is written past it.
 @param input The code points to encode.
 @return The number of characters written, or @ref error::invalid_punycode for
 a surrogate or a value above U+10FFFF, @ref error::punycode_overflow when the
 arithmetic overflows 32 bits, @ref error::output_too_small when `output`
 cannot hold the result.
 @note A surrogate or a value above U+10FFFF is found before anything is
 written, whatever the size of `output`; an overflow is found as the digits
 are written, so an `output` too small for the result may give
 @ref error::output_too_small first. After an error, the contents of `output`
 are unspecified.
 @see "Punycode", in the guide.
*/
[[nodiscard]] inline result<std::size_t> encode(std::span<char> output,
                                                std::u32string_view input) noexcept {
    std::size_t at = 0;
    return detail::punycode::encode(input, [&](char c) noexcept {
        if (at == output.size()) {
            return false;
        }
        output[at++] = c;
        return true;
    });
}

/**
 Decodes Punycode without its "xn--" prefix into code points, into a caller's
 buffer (RFC 3492 section 6.2).

 The code points before the last delimiter are copied as they are, case
 included, as RFC 3492's mixed-case annotation leaves them; the digits after
 it are read in either case. A delimiter at the very start consumes nothing.
 At most the size of `output` code points are decoded, so the decoding's time
 grows at most with the input's length times the size of `output`.

 @param output Where the code points are written; nothing is written past it.
 @param input The Punycode to decode.
 @return The number of code points written, or @ref error::invalid_punycode
 for a character that is not a digit, a non-basic code point before the
 delimiter, a truncated number, or a value that is basic, a surrogate or above
 U+10FFFF; @ref error::punycode_overflow; or @ref error::output_too_small.
 @note A non-basic code point before the delimiter is found before anything is
 written, whatever the size of `output`; an error in the digits is found as
 they are decoded, so an `output` too small for the result may give
 @ref error::output_too_small first. After an error, the contents of `output`
 are unspecified.
 @see "Punycode", in the guide.
*/
[[nodiscard]] inline result<std::size_t> decode(std::span<char32_t> output,
                                                std::string_view input) noexcept {
    return detail::punycode::decode(output, input);
}

/**
 Encodes code points as Punycode into a string of the given memory resource.

 Nothing bounds the input's length, and the encoding's time grows with its
 square, since RFC 3492's encoder passes over the input once for each
 distinct code point: a program bounds the length of input it does not trust,
 as `to_ascii` bounds a label at 256 code points.

 @param input The code points to encode.
 @param resource Where the string's memory comes from.
 @return The Punycode, or the error the span form returns but
 @ref error::output_too_small.
 @see "Allocation", in the guide.
 @see "Limits", in the guide.
*/
[[nodiscard]] inline result<std::pmr::string> encode(
    std::u32string_view input,
    std::pmr::memory_resource& resource = *std::pmr::get_default_resource()) {
    std::pmr::string out(&resource);
    const result<std::size_t> written = detail::punycode::encode(input, [&out](char c) {
        out.push_back(c);
        return true;
    });
    if (written.has_error()) {
        return written.error();
    }
    return out;
}

/**
 Decodes Punycode into a string of code points of the given memory resource.

 Nothing bounds the input's length, and the decoding's time grows with its
 square, since RFC 3492's decoder inserts each code point among those before
 it: a program bounds the length of input it does not trust, as `to_unicode`
 bounds a label at 256 code points.

 @param input The Punycode to decode.
 @param resource Where the string's memory comes from.
 @return The code points, or the error the span form returns but
 @ref error::output_too_small.
 @see "Allocation", in the guide.
 @see "Limits", in the guide.
*/
[[nodiscard]] inline result<std::pmr::u32string> decode(
    std::string_view input,
    std::pmr::memory_resource& resource = *std::pmr::get_default_resource()) {
    // Every code point consumes at least one character, so the input's size bounds the output.
    std::pmr::u32string out(input.size(), U'\0', &resource);
    const result<std::size_t> written = detail::punycode::decode(out, input);
    if (written.has_error()) {
        return written.error();
    }
    out.resize(written.unsafe_value());
    return out;
}

}  // namespace punycode

}  // namespace webcpp::idna

#endif  // WEBCPP_IDNA_PUNYCODE_HPP
