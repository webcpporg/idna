// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The contextual rules of RFC 5892 appendix A, each allowed and refused, over a label's code
// points: the cases of the appendix, and those an implementation that lists the joining scripts by
// hand gets wrong (NKo, Mandaic, Phags-pa, a non-joining neighbour).

#include <webcpp/idna/detail/context.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <string_view>

namespace context = webcpp::idna::detail::context;

namespace {

/** The CONTEXTO code points of RFC 5892 appendix A, each digit range by its two ends. */
constexpr std::array<char32_t, 9> contexto_points = {
    0x00B7,  // MIDDLE DOT, A.3
    0x0375,  // GREEK LOWER NUMERAL SIGN (KERAIA), A.4
    0x05F3,  // HEBREW PUNCTUATION GERESH, A.5
    0x05F4,  // HEBREW PUNCTUATION GERSHAYIM, A.6
    0x30FB,  // KATAKANA MIDDLE DOT, A.7
    0x0660,  // ARABIC-INDIC DIGIT ZERO, A.8
    0x0669,  // ARABIC-INDIC DIGIT NINE, A.8
    0x06F0,  // EXTENDED ARABIC-INDIC DIGIT ZERO, A.9
    0x06F9,  // EXTENDED ARABIC-INDIC DIGIT NINE, A.9
};

/** The code points just outside each digit range, and one of the rules' own letters. */
constexpr std::array<char32_t, 5> not_contexto_points = {
    0x065F,  // before ARABIC-INDIC DIGIT ZERO
    0x066A,  // after ARABIC-INDIC DIGIT NINE
    0x06EF,  // before EXTENDED ARABIC-INDIC DIGIT ZERO
    0x06FA,  // after EXTENDED ARABIC-INDIC DIGIT NINE
    U'l',    // the letter around A.3's MIDDLE DOT
};

/** A.1: after a virama, or between L or D and R or D with only T between, at every occurrence. */
void zero_width_non_joiner() {
    BOOST_TEST(context::check_joiners(U"\u0915\u094D\u200C\u0937"));        // after a virama
    BOOST_TEST(context::check_joiners(U"\u0628\u200C\u0628"));              // D, D
    BOOST_TEST(context::check_joiners(U"\u0628\u200C\u0627"));              // D, R
    BOOST_TEST(context::check_joiners(U"\uA872\u200C\u0628"));              // Phags-pa, L, D
    BOOST_TEST(context::check_joiners(U"\u0628\u064B\u200C\u064B\u0628"));  // T between
    BOOST_TEST(context::check_joiners(U"\u07CA\u200C\u07CA"));              // NKo, D, D
    BOOST_TEST(context::check_joiners(U"\u0841\u200C\u0841"));              // Mandaic, D, D
    BOOST_TEST(!context::check_joiners(U"\u0627\u200C\u0628"));             // R before
    BOOST_TEST(!context::check_joiners(U"\u0628\u200C\uA872"));             // L after
    BOOST_TEST(!context::check_joiners(U"\u0628\u0621\u200C\u0628"));       // U before, HAMZA
    BOOST_TEST(!context::check_joiners(U"\u0628\u200C\u0621\u0628"));       // U after
    BOOST_TEST(!context::check_joiners(U"\u200C\u0628"));
    BOOST_TEST(!context::check_joiners(U"\u0628\u200C"));
    BOOST_TEST(!context::check_joiners(U"a\u200Cb"));
    // The first occurrence is allowed, the second is not.
    BOOST_TEST(!context::check_joiners(U"\u0915\u094D\u200C\u0937x\u200Cy"));
}

/** A.2: only after a virama. */
void zero_width_joiner() {
    BOOST_TEST(context::check_joiners(U"\u0915\u094D\u200D\u0937"));
    BOOST_TEST(!context::check_joiners(U"\u0628\u200D\u0628"));
    BOOST_TEST(!context::check_joiners(U"\u200D"));
}

/** A.3 to A.6: a neighbour, before or after. */
void middle_dot_keraia_geresh() {
    BOOST_TEST(context::check_others(U"l\u00B7l"));
    BOOST_TEST(!context::check_others(U"a\u00B7l"));
    BOOST_TEST(!context::check_others(U"\u00B7l"));
    BOOST_TEST(!context::check_others(U"l\u00B7a"));
    BOOST_TEST(!context::check_others(U"l\u00B7"));
    BOOST_TEST(context::check_others(U"\u03B1\u0375\u03B2"));
    BOOST_TEST(!context::check_others(U"\u03B1\u0375S"));
    BOOST_TEST(!context::check_others(U"\u03B1\u0375"));
    BOOST_TEST(context::check_others(U"\u05D1\u05F3"));
    BOOST_TEST(context::check_others(U"\u05D1\u05F4"));
    BOOST_TEST(!context::check_others(U"\u05F3\u05D1"));
    BOOST_TEST(!context::check_others(U"a\u05F4"));
    // The first occurrence is allowed, the second is not.
    BOOST_TEST(!context::check_others(U"l\u00B7la\u05F3"));
}

/** A.7 to A.9: the whole label. */
void katakana_middle_dot_and_digits() {
    BOOST_TEST(context::check_others(U"\u30FB\u3041"));  // Hiragana
    BOOST_TEST(context::check_others(U"\u30A2\u30FB"));  // Katakana
    BOOST_TEST(context::check_others(U"\u4E00\u30FB"));  // Han
    BOOST_TEST(!context::check_others(U"def\u30FBabc"));
    BOOST_TEST(!context::check_others(U"\u03B1\u30FB\u05D0"));  // Greek and Hebrew, named too
    BOOST_TEST(!context::check_others(U"\u30FB"));
    BOOST_TEST(context::check_others(U"\u0660\u0661"));
    BOOST_TEST(context::check_others(U"\u06F0\u06F1"));
    BOOST_TEST(context::check_others(U"\u0628\u0660"));  // a letter is no digit
    BOOST_TEST(!context::check_others(U"\u0660\u06F0"));
    BOOST_TEST(!context::check_others(U"\u06F9\u0669"));
}

/** A.8 and A.9 each on its own: a label with both digits breaks both, so the label alone cannot
    tell which rule refused it. */
void each_digit_rule() {
    BOOST_TEST(context::other_allowed(U"\u0660\u0661", 0));
    BOOST_TEST(context::other_allowed(U"\u06F0\u06F1", 0));
    BOOST_TEST(!context::other_allowed(U"\u0660\u06F0", 0));  // A.8
    BOOST_TEST(!context::other_allowed(U"\u06F0\u0660", 0));  // A.9
}

void the_sets() {
    BOOST_TEST(context::is_contextj(0x200C) && context::is_contextj(0x200D));
    BOOST_TEST(!context::is_contextj(0x200B));
    BOOST_TEST(!context::is_contextj(0x200E));
    for (const char32_t cp : contexto_points) {
        BOOST_TEST(context::is_contexto(cp));
    }
    for (const char32_t cp : not_contexto_points) {
        BOOST_TEST(!context::is_contexto(cp));
    }
}

}  // namespace

int main() {
    zero_width_non_joiner();
    zero_width_joiner();
    middle_dot_keraia_geresh();
    katakana_middle_dot_and_digits();
    each_digit_rule();
    the_sets();
    return boost::report_errors();
}
