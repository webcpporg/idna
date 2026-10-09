// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

/**
 The contextual rules of RFC 5892 appendix A over a label's code points:
 CONTEXTJ (A.1, A.2, IdnaTestV2's C1 and C2) for the joiners, CONTEXTO (A.3 to
 A.9) for the others.

 Every code point of a label is at most U+10FFFF, the domain of the lookups of
 detail/properties.hpp, as the UTF-8 and Punycode decoders guarantee.
*/
#ifndef WEBCPP_IDNA_DETAIL_CONTEXT_HPP
#define WEBCPP_IDNA_DETAIL_CONTEXT_HPP

#include <webcpp/idna/config.hpp>

#include <webcpp/idna/detail/properties.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace webcpp::idna::detail::context {

/** The combining class of a virama, which A.1 and A.2 accept before a joiner. */
inline constexpr std::uint8_t virama = 9;

/** Whether `cp` is an ARABIC-INDIC DIGIT, U+0660 to U+0669. */
constexpr bool is_arabic_indic_digit(char32_t cp) noexcept {
    return cp >= 0x0660 && cp <= 0x0669;
}

/** Whether `cp` is an EXTENDED ARABIC-INDIC DIGIT, U+06F0 to U+06F9. */
constexpr bool is_extended_arabic_indic_digit(char32_t cp) noexcept {
    return cp >= 0x06F0 && cp <= 0x06F9;
}

/** Whether `cp` is one of the CONTEXTJ code points, U+200C and U+200D. */
constexpr bool is_contextj(char32_t cp) noexcept {
    return cp == 0x200C || cp == 0x200D;
}

/** Whether `cp` is one of the CONTEXTO code points RFC 5892 appendix A gives a rule. */
constexpr bool is_contexto(char32_t cp) noexcept {
    return cp == 0x00B7 || cp == 0x0375 || cp == 0x05F3 || cp == 0x05F4 || cp == 0x30FB ||
           is_arabic_indic_digit(cp) || is_extended_arabic_indic_digit(cp);
}

/**
 A.1's regular expression around the ZERO WIDTH NON-JOINER at `at`: an L or D before it and an R
 or D after it, with only T between.

 Tip: read from the Joining_Type table, never from a list of joining scripts, which misses those
 it forgets and those a later Unicode adds, as NKo, Mandaic and Phags-pa show in the tests.
*/
constexpr bool joins_around(std::u32string_view label, std::size_t at) noexcept {
    std::size_t before = at;
    tables::joining_type type = tables::joining_type::t;
    while (type == tables::joining_type::t) {
        if (before == 0) {
            return false;
        }
        type = joining_type_of(label[--before]);
    }
    if (type != tables::joining_type::l && type != tables::joining_type::d) {
        return false;
    }
    std::size_t after = at;
    type = tables::joining_type::t;
    while (type == tables::joining_type::t) {
        if (++after == label.size()) {
            return false;
        }
        type = joining_type_of(label[after]);
    }
    return type == tables::joining_type::r || type == tables::joining_type::d;
}

/** Whether the joiner at `at` satisfies A.1 (U+200C) or A.2 (U+200D). */
constexpr bool joiner_allowed(std::u32string_view label, std::size_t at) noexcept {
    if (at > 0 && combining_class_of(label[at - 1]) == virama) {
        return true;
    }
    return label[at] == 0x200C && joins_around(label, at);
}

/** Whether every joiner of `label` satisfies its rule, A.1 or A.2, at each of its occurrences. */
constexpr bool check_joiners(std::u32string_view label) noexcept {
    for (std::size_t at = 0; at < label.size(); ++at) {
        if (is_contextj(label[at]) && !joiner_allowed(label, at)) {
            return false;
        }
    }
    return true;
}

/** Whether `cp` is of a script A.7 accepts in a label with a KATAKANA MIDDLE DOT. */
constexpr bool is_kana_or_han(char32_t cp) noexcept {
    const tables::script s = script_of(cp);
    return s == tables::script::hiragana || s == tables::script::katakana ||
           s == tables::script::han;
}

/** What A.7 to A.9 ask of a whole label, gathered once for all their occurrences. */
struct label_facts {
    /** Whether the label holds a Hiragana, Katakana or Han code point (A.7). */
    bool kana_or_han = false;

    /** Whether the label holds an ARABIC-INDIC DIGIT (A.9). */
    bool arabic_indic_digit = false;

    /** Whether the label holds an EXTENDED ARABIC-INDIC DIGIT (A.8). */
    bool extended_arabic_indic_digit = false;
};

/** The facts of `label`, in one pass over it, which reads a code point's script only until one of
    A.7's scripts is found. */
constexpr label_facts facts_of(std::u32string_view label) noexcept {
    label_facts facts;
    for (const char32_t cp : label) {
        facts.kana_or_han = facts.kana_or_han || is_kana_or_han(cp);
        facts.arabic_indic_digit = facts.arabic_indic_digit || is_arabic_indic_digit(cp);
        facts.extended_arabic_indic_digit =
            facts.extended_arabic_indic_digit || is_extended_arabic_indic_digit(cp);
    }
    return facts;
}

/**
 Whether the CONTEXTO code point at `at` satisfies its rule, A.3 to A.9, given the `facts` of the
 whole label.

 Tip: A.3 to A.6 read a neighbour and A.7 to A.9 read the facts, so each occurrence costs the
 same whatever the label's length.
*/
constexpr bool other_allowed(std::u32string_view label, std::size_t at,
                             const label_facts& facts) noexcept {
    const char32_t cp = label[at];
    const bool has_before = at > 0;
    const bool has_after = at + 1 < label.size();
    switch (cp) {
        case 0x00B7:
            return has_before && has_after && label[at - 1] == U'l' && label[at + 1] == U'l';
        case 0x0375: return has_after && script_of(label[at + 1]) == tables::script::greek;
        case 0x05F3:
        case 0x05F4: return has_before && script_of(label[at - 1]) == tables::script::hebrew;
        case 0x30FB: return facts.kana_or_han;
        default: break;
    }
    if (is_arabic_indic_digit(cp)) {
        return !facts.extended_arabic_indic_digit;
    }
    return !facts.arabic_indic_digit;
}

/**
 Whether every CONTEXTO code point of `label` satisfies its rule, A.3 to A.9, in time linear in
 the label's length.

 Tip: a label without a CONTEXTO code point returns after one pass, and one with them takes a
 second pass for its facts, never a pass per occurrence: a pmr form may hold a label far beyond a
 DNS label's length.
*/
constexpr bool check_others(std::u32string_view label) noexcept {
    if (std::ranges::none_of(label, is_contexto)) {
        return true;
    }
    const label_facts facts = facts_of(label);
    for (std::size_t at = 0; at < label.size(); ++at) {
        if (is_contexto(label[at]) && !other_allowed(label, at, facts)) {
            return false;
        }
    }
    return true;
}

}  // namespace webcpp::idna::detail::context

#endif  // WEBCPP_IDNA_DETAIL_CONTEXT_HPP
