// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The errors idna reports, as `boost::system::error_code` values of a category
 of its own, and the result type of every operation that can fail.

 @see "Errors", in the guide.
*/
#ifndef WEBCPP_IDNA_ERRORS_HPP
#define WEBCPP_IDNA_ERRORS_HPP

#include <webcpp/idna/config.hpp>

#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace webcpp::idna {

/**
 The errors idna reports, as `boost::system::error_code` values of the category
 @ref error_category.

 One enumerator per rule family, not per UTS 46 status code: the comment of
 each names the steps of UTS 46, the status codes of IdnaTestV2 and the
 sections of the RFCs it reports. The values start at 1, so an `error_code` of
 value 0 is no error.

 @note The values are fixed: they are never reused for a different meaning,
 so a stored value keeps its meaning across versions.
 @see "Errors", in the guide.
*/
enum class error : int {
    /** The input is not well-formed UTF-8 (RFC 3629). */
    invalid_utf8 = 1,

    /** A code point whose UTS 46 status is neither valid nor, in nontransitional processing,
        deviation (UTS 46 V7), or a U+002E FULL STOP inside a label (UTS 46 V5). */
    disallowed = 2,

    /** A code point UTS 46 marks NV8 or XV8: DISALLOWED in IDNA2008 (options::idna2008). */
    not_idna2008 = 3,

    /** An ASCII code point outside a-z, 0-9 and the hyphen (UTS 46 U1,
        options::use_std3_ascii_rules). */
    not_ldh = 4,

    /** A label not in Normalization Form C (UTS 46 V1, RFC 5891 section 5.4). */
    not_nfc = 5,

    /** Hyphens in both the third and the fourth positions (UTS 46 V2, options::check_hyphens). */
    hyphen_in_third_and_fourth = 6,

    /** A hyphen at the start or the end of a label (UTS 46 V3, options::check_hyphens). */
    hyphen_at_start_or_end = 7,

    /** A label that starts with "xn--" while options::check_hyphens is off (UTS 46 V4). */
    ace_prefix = 8,

    /** A label that starts with a combining mark (UTS 46 V6, RFC 5891 section 5.4). */
    leading_combining_mark = 9,

    /** A joiner outside the context RFC 5892 A.1 and A.2 allow (IdnaTestV2 C1, C2,
        options::check_joiners). */
    contextj = 10,

    /** A code point outside the context RFC 5892 A.3 to A.9 allow (options::idna2008). */
    contexto = 11,

    /** A Bidi domain name with a label that breaks RFC 5893 section 2 (IdnaTestV2 B1 to
        B6, options::check_bidi). */
    bidi = 12,

    /** Punycode that does not decode, an A-label that decodes to nothing or to ASCII alone, or
        one not in its canonical form (RFC 3492, UTS 46 P4, RFC 5891 section 5.4). */
    invalid_punycode = 13,

    /** Punycode whose arithmetic overflows 32 bits (RFC 3492 section 6.4, UTS 46 A3), or, in the
        pmr forms of to_ascii and to_unicode, a label whose Punycode would hold more than 256 code
        points. */
    punycode_overflow = 14,

    /** An empty label (UTS 46 A4_2 and X4_2, options::verify_dns_length). */
    empty_label = 15,

    /** A label longer than 63 octets in its ASCII form (UTS 46 A4_2,
        options::verify_dns_length). */
    label_too_long = 16,

    /** A domain name longer than 253 octets in its ASCII form (UTS 46 A4_1,
        options::verify_dns_length). */
    domain_too_long = 17,

    /** The caller's span cannot hold the result. */
    output_too_small = 18,

    /** A label beyond the inline working storage, with no memory resource to hold it. */
    storage_exhausted = 19,
};

namespace detail {

/** The human message of an @ref error value, or the unknown message for a value it does not
    define. */
inline const char* message_of(int value) noexcept {
    switch (static_cast<error>(value)) {
        case error::invalid_utf8: return "invalid UTF-8";
        case error::disallowed: return "disallowed code point";
        case error::not_idna2008: return "code point disallowed by IDNA2008";
        case error::not_ldh: return "ASCII code point other than a letter, a digit or a hyphen";
        case error::not_nfc: return "label not in Normalization Form C";
        case error::hyphen_in_third_and_fourth: return "hyphens in the third and fourth positions";
        case error::hyphen_at_start_or_end: return "hyphen at the start or end of a label";
        case error::ace_prefix: return "label starts with xn--";
        case error::leading_combining_mark: return "label starts with a combining mark";
        case error::contextj: return "joiner outside its allowed context";
        case error::contexto: return "code point outside its allowed context";
        case error::bidi: return "Bidi rule violated";
        case error::invalid_punycode: return "invalid Punycode";
        case error::punycode_overflow: return "Punycode overflow";
        case error::empty_label: return "empty label";
        case error::label_too_long: return "label longer than 63 octets";
        case error::domain_too_long: return "domain name longer than 253 octets";
        case error::output_too_small: return "output too small";
        case error::storage_exhausted: return "label beyond the working storage";
    }
    return "unknown webcpp.idna error";
}

/**
 A constant category with a fixed identity, so idna keeps no mutable state of its own.

 @note The identity is the ASCII of "idna" followed by four zero bytes.
*/
class category final : public boost::system::error_category {
public:
    /** Constructs the category with its fixed identity. */
    constexpr category() noexcept : boost::system::error_category(0x69646E6100000000ULL) {}

    /** The category's name, "webcpp.idna". */
    const char* name() const noexcept override { return "webcpp.idna"; }

    /** The human message of the error `value`, as @ref message_of spells it. */
    std::string message(int value) const override { return message_of(value); }

    /**
     Copies the human message of the error `value` into `buffer`, cut to fit `size` bytes with
     its terminating zero, and returns the whole message.
    */
    const char* message(int value, char* buffer, std::size_t size) const noexcept override {
        const char* text = message_of(value);
        if (buffer == nullptr || size == 0) {
            return text;
        }
        const std::string_view spelling(text);
        const std::size_t limit = size - 1;
        const std::size_t count = spelling.size() < limit ? spelling.size() : limit;
        std::memcpy(buffer, spelling.data(), count);
        buffer[count] = '\0';
        return text;
    }
};

/** The one category of idna's errors, which @ref error_category returns. */
inline constexpr category the_category{};

}  // namespace detail

/**
 The category of every @ref error.

 Its `name()` is "webcpp.idna", and its `message(value)` the human message of
 the error, "invalid Punycode" for @ref error::invalid_punycode, or "unknown
 webcpp.idna error" for a value @ref error does not define.

 @return The category, a constant with a fixed identity.
 @see "Errors", in the guide.
*/
inline const boost::system::error_category& error_category() noexcept {
    return detail::the_category;
}

/**
 Makes the `error_code` of an @ref error, as its implicit conversion does.

 @param e The error.
 @return `e` as an `error_code` of the category @ref error_category.
*/
inline boost::system::error_code make_error_code(error e) noexcept {
    return {static_cast<int>(e), error_category()};
}

/**
 What an operation that can fail returns: a `T`, or the `error_code` of the
 failure.

 It is Boost.System's `result`. idna throws nothing of its own: every
 operation of it that can fail returns one, whose error is an @ref error.

 @tparam T The type of the value; `void` for an operation that returns none.
 @see "Errors", in the guide.
*/
template <class T>
using result = boost::system::result<T>;

}  // namespace webcpp::idna

namespace boost::system {
template <>
struct is_error_code_enum<webcpp::idna::error> : std::true_type {};
}  // namespace boost::system

#endif  // WEBCPP_IDNA_ERRORS_HPP
