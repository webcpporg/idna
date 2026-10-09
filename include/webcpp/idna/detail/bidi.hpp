// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The Bidi rule of RFC 5893 section 2 over one label, its rules 1 to 6 being
 IdnaTestV2's B1 to B6, and what a label tells of its domain: whether it makes
 the domain a Bidi domain name.

 Every code point of a label is at most U+10FFFF, the domain of the lookups of
 detail/properties.hpp, as the UTF-8 and Punycode decoders guarantee.
*/
#ifndef WEBCPP_IDNA_DETAIL_BIDI_HPP
#define WEBCPP_IDNA_DETAIL_BIDI_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/properties.hpp>

#include <cstdint>
#include <string_view>

namespace webcpp::idna::detail::bidi {

/** What one label says of the Bidi rule. */
struct label_bidi {
    /** Whether the label holds an R, AL or AN code point, which makes its domain a Bidi domain
        name (RFC 5893 section 1.4). */
    bool rtl = false;

    /** Whether the label satisfies the six rules of RFC 5893 section 2. */
    bool satisfied = true;
};

/** The one-bit set of the Bidi class `c`, 32 bits wide so that no operand is promoted to int. */
constexpr std::uint32_t bit(tables::bidi_class c) noexcept {
    return 1U << static_cast<std::uint32_t>(c);
}

/** R, AL and AN: a label that holds one is an RTL label. */
inline constexpr std::uint32_t right_to_left =
    bit(tables::bidi_class::r) | bit(tables::bidi_class::al) | bit(tables::bidi_class::an);

/** The classes RFC 5893 rule 2 (B2) allows in an RTL label. */
inline constexpr std::uint32_t rtl_allowed =
    right_to_left | bit(tables::bidi_class::en) | bit(tables::bidi_class::es) |
    bit(tables::bidi_class::cs) | bit(tables::bidi_class::et) | bit(tables::bidi_class::on) |
    bit(tables::bidi_class::bn) | bit(tables::bidi_class::nsm);

/** The classes RFC 5893 rule 5 (B5) allows in an LTR label. */
inline constexpr std::uint32_t ltr_allowed =
    bit(tables::bidi_class::l) | bit(tables::bidi_class::en) | bit(tables::bidi_class::es) |
    bit(tables::bidi_class::cs) | bit(tables::bidi_class::et) | bit(tables::bidi_class::on) |
    bit(tables::bidi_class::bn) | bit(tables::bidi_class::nsm);

/** The classes an RTL label may end with before its NSMs (rule 3, B3). */
inline constexpr std::uint32_t rtl_ends = bit(tables::bidi_class::r) | bit(tables::bidi_class::al) |
                                          bit(tables::bidi_class::en) | bit(tables::bidi_class::an);

/** The classes an LTR label may end with before its NSMs (rule 6, B6). */
inline constexpr std::uint32_t ltr_ends = bit(tables::bidi_class::l) | bit(tables::bidi_class::en);

/** EN and AN, which an RTL label may not hold both of (rule 4, B4). */
inline constexpr std::uint32_t numbers = bit(tables::bidi_class::en) | bit(tables::bidi_class::an);

/**
 RFC 5893 section 2 over `label`; an empty label is neither RTL nor in breach of a rule, since
 whether a label may be empty is its caller's question (UTS 46's empty label).

 Tip: the rules' direction is that of the first code point, R or AL for RTL, as rule 1 (B1) leaves
 only L, R and AL there; a label that starts with L and holds R, AL or AN is RTL in RFC 5893's
 terms and fails rule 2, and fails rule 5 here, so the verdict is the same.
*/
constexpr label_bidi check(std::u32string_view label) noexcept {
    if (label.empty()) {
        return {};
    }
    std::uint32_t seen = 0;
    tables::bidi_class last = tables::bidi_class::other;
    for (const char32_t cp : label) {
        const tables::bidi_class c = bidi_class_of(cp);
        seen |= bit(c);
        last = c == tables::bidi_class::nsm ? last : c;
    }
    label_bidi found{.rtl = (seen & right_to_left) != 0, .satisfied = true};
    const tables::bidi_class first = bidi_class_of(label.front());
    const bool rtl_rules = first == tables::bidi_class::r || first == tables::bidi_class::al;
    if (!rtl_rules && first != tables::bidi_class::l) {
        found.satisfied = false;  // rule 1
    } else if (rtl_rules) {
        found.satisfied = (seen & ~rtl_allowed) == 0 && (bit(last) & rtl_ends) != 0 &&
                          (seen & numbers) != numbers;
    } else {
        found.satisfied = (seen & ~ltr_allowed) == 0 && (bit(last) & ltr_ends) != 0;
    }
    return found;
}

}  // namespace webcpp::idna::detail::bidi

#endif  // WEBCPP_IDNA_DETAIL_BIDI_HPP
