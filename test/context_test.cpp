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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <string>
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
    // C, join-causing, is neither L nor D (IdnaTestV2 C1).
    BOOST_TEST(!context::check_joiners(U"\u0640\u200C\u0628"));
    BOOST_TEST(!context::check_joiners(U"\u0628\u200C\u0640"));
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

/** What A.7 to A.9 read of a whole label, gathered in one pass. */
void the_facts_of_a_label() {
    const context::label_facts none = context::facts_of(U"abc\u0628");
    BOOST_TEST(!none.kana_or_han);
    BOOST_TEST(!none.arabic_indic_digit);
    BOOST_TEST(!none.extended_arabic_indic_digit);
    BOOST_TEST(context::facts_of(U"a\u3041").kana_or_han);
    BOOST_TEST(context::facts_of(U"\u30A2").kana_or_han);
    BOOST_TEST(context::facts_of(U"\u4E00").kana_or_han);
    BOOST_TEST(!context::facts_of(U"\u30FB\u03B1\u05D0").kana_or_han);
    const context::label_facts digits = context::facts_of(U"\u0669a\u06F9");
    BOOST_TEST(digits.arabic_indic_digit);
    BOOST_TEST(digits.extended_arabic_indic_digit);
    BOOST_TEST(!context::facts_of(U"\u0669").extended_arabic_indic_digit);
    BOOST_TEST(!context::facts_of(U"\u06F9").arabic_indic_digit);
    // A fact holds once found, whatever follows it.
    BOOST_TEST(context::facts_of(U"\u3041a").kana_or_han);
    BOOST_TEST(context::facts_of(U"\u0669a").arabic_indic_digit);
    BOOST_TEST(context::facts_of(U"\u06F9a").extended_arabic_indic_digit);
}

/** A.7 to A.9 each on its own, from the facts: a label with both digits breaks A.8 and A.9
    together, so the label alone cannot tell which rule refused it. */
void each_whole_label_rule() {
    constexpr context::label_facts nothing{};
    constexpr context::label_facts kana{.kana_or_han = true};
    constexpr context::label_facts arabic{.arabic_indic_digit = true};
    constexpr context::label_facts extended{.extended_arabic_indic_digit = true};
    constexpr context::label_facts both{
        .arabic_indic_digit = true,
        .extended_arabic_indic_digit = true,
    };
    BOOST_TEST(context::other_allowed(U"\u30FB", 0, kana));      // A.7
    BOOST_TEST(!context::other_allowed(U"\u30FB", 0, nothing));  // A.7
    BOOST_TEST(context::other_allowed(U"\u0660", 0, arabic));    // A.8
    BOOST_TEST(!context::other_allowed(U"\u0660", 0, both));     // A.8
    BOOST_TEST(context::other_allowed(U"\u06F0", 0, extended));  // A.9
    BOOST_TEST(!context::other_allowed(U"\u06F0", 0, both));     // A.9
}

/** The code points of the longest labels below: a pass per occurrence of A.7 to A.9 over the whole
    label takes seconds there, one pass over the label a fraction of a millisecond. */
constexpr std::size_t long_label = 8192;

/** `n` copies of `cp`, then `last`. */
std::u32string repeated(std::size_t n, char32_t cp, char32_t last) {
    std::u32string label(n, cp);
    label.push_back(last);
    return label;
}

/** A.7 to A.9 over labels far beyond a DNS label, as a pmr form may hold them. */
void long_labels() {
    BOOST_TEST(context::check_others(repeated(4096, 0x30FB, 0x3041)));
    BOOST_TEST(!context::check_others(repeated(4096, 0x30FB, U'a')));
    BOOST_TEST(context::check_others(repeated(4096, 0x0660, 0x0661)));
    BOOST_TEST(!context::check_others(repeated(4096, 0x0660, 0x06F0)));
    BOOST_TEST(!context::check_others(repeated(4096, 0x06F0, 0x0660)));
}

/** The nanoseconds of the fastest of three runs of `check_others` over `label`, each of which must
    give `expected`. */
long long fastest_of_three(const std::u32string& label, bool expected) {
    auto fastest = std::chrono::nanoseconds::max();
    for (int i = 0; i < 3; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const bool allowed = context::check_others(label);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        BOOST_TEST_EQ(allowed, expected);
        fastest = std::min(fastest, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    return fastest.count();
}

/**
 Whether `check_others` of `label` gives `expected` in less than 1,000 times the time it takes over
 a label as long without a CONTEXTO code point, the fastest of three runs each: one pass over the
 label keeps that ratio near 13 on any machine, and a pass per occurrence takes it past 30,000.

 Tip: the baseline takes microseconds, so a loaded machine has taken the ratio to 41 (-O3, 2 x 50
 rounds beside a build of 18 jobs); 1,000 leaves room for that 24 times over, and still catches the
 pass per occurrence 30 times over.
*/
void within_a_ratio(const std::u32string& label, bool expected) {
    const long long baseline = fastest_of_three(std::u32string(label.size(), U'a'), true);
    const long long measured = fastest_of_three(label, expected);
    BOOST_TEST_LT(measured, 1000 * baseline);
}

/** A.7 to A.9 in one pass over the label, whatever the number of their occurrences. */
void long_labels_in_linear_time() {
    within_a_ratio(repeated(long_label, 0x30FB, 0x3041), true);
    within_a_ratio(repeated(4 * long_label, 0x0660, 0x0661), true);
    within_a_ratio(repeated(4 * long_label, 0x06F0, 0x06F1), true);
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
    the_facts_of_a_label();
    each_whole_label_rule();
    long_labels();
    long_labels_in_linear_time();
    the_sets();
    return boost::report_errors();
}
