// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// UTS 46 processing against every line of IdnaTestV2.txt, toUnicode, toAsciiN and toAsciiT, in
// both the span and the pmr forms: first with options::idna2008 off and every other flag on, as the
// file assumes, then the strict profile over the lines whose toUnicode field holds no NV8, XV8 or
// CONTEXTO code point. A line of test/known_failures.json must fail, and every other must pass.
// Then the orders of the errors, the storage, the bound on a label's Punycode in the pmr forms, and
// what each form may throw. Every target runs the same lines and asks the same outputs of them:
// the page's guarantee 3, idna-invariant-3.

#include <webcpp/idna/detail/utf8.hpp>
#include <webcpp/idna/uts46.hpp>

#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "idna_vectors.hpp"
#include "known_failures.hpp"

namespace idna = webcpp::idna;
using webcpp::test::idna_vector;
using webcpp::test::idna_vectors;

namespace {

// The span forms throw nothing; the pmr forms may throw what their memory resource throws
// (idna-invariant-2).
static_assert(noexcept(idna::to_ascii(std::declval<std::span<char>>(), std::string_view())));
static_assert(noexcept(idna::to_unicode(std::declval<std::span<char>>(), std::string_view())));
static_assert(!noexcept(idna::to_ascii(std::string_view())));
static_assert(!noexcept(idna::to_unicode(std::string_view())));

/** Room for every result of the file, whose longest line is far below it. */
using buffer = std::array<char, 1024>;

/** Whether ToASCII gives `expected`, or an error when `error_expected`, in both forms. */
bool to_ascii_agrees(std::string_view source, const idna::options& opts, std::string_view expected,
                     bool error_expected) {
    buffer out{};
    const idna::result<std::size_t> span = idna::to_ascii(out, source, opts);
    const idna::result<std::pmr::string> owned = idna::to_ascii(source, opts);
    if (span.has_value() != owned.has_value() || span.has_error() != error_expected) {
        return false;
    }
    if (error_expected) {
        return span.error() == owned.error();
    }
    const std::string_view written(out.data(), span.value());
    return written == expected && owned.value() == expected;
}

/** Whether ToUnicode gives `expected`, with an error when `error_expected`, in both forms. A
    source that is not well-formed UTF-8 (a lone surrogate of the file) writes nothing and is
    invalid_utf8, which IdnaTestV2 allows ("Implementations that cannot work with ill-formed
    strings would skip test cases that contain them"). */
bool to_unicode_agrees(std::string_view source, const idna::options& opts,
                       std::string_view expected, bool error_expected) {
    buffer out{};
    const idna::unicode_result span = idna::to_unicode(out, source, opts);
    const idna::unicode_string owned = idna::to_unicode(source, opts);
    if (!idna::detail::utf8::is_valid(source)) {
        return span.size == 0 && span.error == idna::error::invalid_utf8 && owned.value.empty() &&
               owned.error == idna::error::invalid_utf8;
    }
    const std::string_view written(out.data(), span.size);
    return written == expected && owned.value == expected && span.error == owned.error &&
           static_cast<bool>(span.error) == error_expected;
}

/** Whether every column of `line` holds under `opts`. */
bool line_holds(const idna_vector& line, const idna::options& opts) {
    idna::options transitional = opts;
    transitional.transitional_processing = true;
    return to_unicode_agrees(line.source, opts, line.to_unicode, line.to_unicode_error) &&
           to_ascii_agrees(line.source, opts, line.to_ascii_n, line.to_ascii_n_error) &&
           to_ascii_agrees(line.source, transitional, line.to_ascii_t, line.to_ascii_t_error);
}

/** Every line under UTS 46 alone: a line holds unless it is a known failure, which must fail, so
    the list never lies. */
void idna_test_v2() {
    const idna::options uts46_alone{.idna2008 = false};
    for (const idna_vector& line : idna_vectors) {
        webcpp::test::as_listed(line.line, line.known_failure, line_holds(line, uts46_alone));
    }
}

/** The strict pass leaves out a known failure, whose entry is the UTS 46 pass's: a line may fail
    there for a reason the strict pass never meets. */
void idna_test_v2_strict() {
    for (const idna_vector& line : idna_vectors) {
        if (line.strict && line.known_failure.empty()) {
            webcpp::test::as_listed(line.line, line.known_failure,
                                    line_holds(line, idna::options()));
        }
    }
}

/** `count` labels of nine letters, joined by dots: 10 * count - 1 octets. */
std::string nine_letter_labels(int count) {
    std::string name = "abcdefghi";
    for (int i = 1; i < count; ++i) {
        name += ".abcdefghi";
    }
    return name;
}

/**
 With verify_dns_length, to_ascii writes at most 253 chars, and a name whose ASCII form is longer
 is domain_too_long, never output_too_small, in the 253 chars the guide says always suffice, unless
 an earlier error comes first: a label's, then the Bidi rule's (idna-invariant-4).
*/
void the_length_comes_before_the_buffer() {
    std::array<char, idna::max_domain_length> out{};
    const std::string longest = nine_letter_labels(25) + ".abc";
    static_assert(idna::max_domain_length == 253);
    const idna::result<std::size_t> fits = idna::to_ascii(out, longest);
    BOOST_TEST(fits.has_value() && fits.value() == 253U &&
               std::string_view(out.data(), fits.value()) == longest);
    BOOST_TEST(idna::to_ascii(out, longest + "d").error() == idna::error::domain_too_long);
    BOOST_TEST(idna::to_ascii(out, nine_letter_labels(30) + ".x").error() ==
               idna::error::domain_too_long);
    // 20 labels of 7 octets each, "b\303\274cher", are 159 octets, and 279 in their ASCII form.
    std::string grows = "b\303\274cher";
    for (int i = 1; i < 20; ++i) {
        grows += ".b\303\274cher";
    }
    BOOST_TEST(idna::to_ascii(out, grows).error() == idna::error::domain_too_long);
    BOOST_TEST(idna::to_ascii(out, "a_b." + nine_letter_labels(30)).error() ==
               idna::error::not_ldh);
    BOOST_TEST(idna::to_ascii(out, "0a.\327\220." + nine_letter_labels(30)).error() ==
               idna::error::bidi);
    std::array<char, 5> tiny{};
    BOOST_TEST(idna::to_ascii(tiny, "b\303\274cher.example").error() ==
               idna::error::output_too_small);
}

/** ToUnicode writes whole code points, and says when the buffer could not hold the result, before
    any error the processing recorded. */
void to_unicode_stops_at_a_code_point() {
    std::array<char, 2> out{};
    const idna::unicode_result cut = idna::to_unicode(out, "b\303\274cher");
    BOOST_TEST_EQ(cut.size, 1U);
    BOOST_TEST(cut.error == idna::error::output_too_small);
    const idna::unicode_result cut_after_error = idna::to_unicode(out, "a_b");
    BOOST_TEST_EQ(std::string_view(out.data(), cut_after_error.size), "a_");
    BOOST_TEST(cut_after_error.error == idna::error::output_too_small);
}

/** ToUnicode converts as much as it can: an error does not stop it (idna-invariant-4). */
void to_unicode_goes_on() {
    buffer out{};
    const idna::unicode_result result = idna::to_unicode(out, "A..xn--bcher-kva");
    BOOST_TEST_EQ(std::string_view(out.data(), result.size), "a..b\303\274cher");
    BOOST_TEST(result.error == idna::error::empty_label);
    const idna::unicode_result ill_formed = idna::to_unicode(out, "a\303");
    BOOST_TEST_EQ(ill_formed.size, 0U);
    BOOST_TEST(ill_formed.error == idna::error::invalid_utf8);
    // "xn--" decodes to nothing (IdnaTestV2.txt line 550): an error, and nothing read of it.
    const idna::unicode_result empty = idna::to_unicode(out, "xn--");
    BOOST_TEST_EQ(empty.size, 0U);
    BOOST_TEST(empty.error == idna::error::invalid_punycode);
}

/** The strict profile's defaults, as idn-hostname asks: STD3 rules, the DNS lengths, IDNA2008. */
void the_defaults() {
    buffer out{};
    BOOST_TEST(idna::to_ascii(out, "a_b.example").error() == idna::error::not_ldh);
    BOOST_TEST(idna::to_ascii(out, "example.").error() == idna::error::empty_label);
    BOOST_TEST(idna::to_ascii(out, "\342\231\245.example").error() == idna::error::not_idna2008);
    const idna::result<std::size_t> mapped = idna::to_ascii(out, "B\303\234CHER.Example");
    BOOST_TEST_EQ(std::string_view(out.data(), mapped.value()), "xn--bcher-kva.example");
    // Transitional processing maps U+1E9E, capital sharp s, to "ss".
    const idna::result<std::size_t> sharp =
        idna::to_ascii(out, "\341\272\236", idna::options{.transitional_processing = true});
    BOOST_TEST_EQ(std::string_view(out.data(), sharp.value()), "ss");
}

/** An "xn--" label that does not decode is kept as it is, with no error, under
    ignore_invalid_punycode. */
void ignore_invalid_punycode() {
    buffer out{};
    BOOST_TEST(idna::to_ascii(out, "xn--zz.example").error() == idna::error::invalid_punycode);
    const idna::result<std::size_t> kept =
        idna::to_ascii(out, "xn--zz.example", idna::options{.ignore_invalid_punycode = true});
    BOOST_TEST_EQ(std::string_view(out.data(), kept.value()), "xn--zz.example");
}

/** With verify_dns_length off, a label beyond the inline storage is storage_exhausted in the span
    form, and converted by the pmr form, whose resource holds it. */
void beyond_the_inline_storage() {
    const std::string long_label(300, 'a');
    const idna::options no_lengths{.verify_dns_length = false};
    std::array<char, 400> out{};
    BOOST_TEST(idna::to_ascii(out, long_label, no_lengths).error() ==
               idna::error::storage_exhausted);
    BOOST_TEST(idna::to_ascii(out, long_label).error() == idna::error::label_too_long);
    const idna::result<std::pmr::string> owned = idna::to_ascii(long_label, no_lengths);
    BOOST_TEST(owned.has_value() && std::string_view(owned.value()) == long_label);
    // The storage ends the processing, but keeps the error recorded before it; and it is found
    // while the label is gathered, before the label's own checks (guide, "Limits").
    const idna::unicode_result after_error = idna::to_unicode(out, "a_b." + long_label);
    BOOST_TEST(after_error.error == idna::error::not_ldh);
    BOOST_TEST_EQ(std::string_view(out.data(), after_error.size), "a_b");
    BOOST_TEST(idna::to_ascii(out, "\357\277\277" + long_label).error() ==
               idna::error::label_too_long);
}

/** The four label separators of UTS 46: U+002E, U+3002, U+FF0E and U+FF61. */
constexpr std::array<std::string_view, 4> label_separators = {
    ".",
    "\343\200\202",
    "\357\274\216",
    "\357\275\241",
};

/** Each separator alone or at an edge leaves an empty label, and each between two labels splits
    them. */
void separators() {
    buffer out{};
    for (const std::string_view stop : label_separators) {
        const std::string alone(stop);
        BOOST_TEST(idna::to_ascii(out, alone).error() == idna::error::empty_label);
        BOOST_TEST(idna::to_ascii(out, alone + "example").error() == idna::error::empty_label);
        BOOST_TEST(idna::to_ascii(out, "example" + alone).error() == idna::error::empty_label);
        const idna::result<std::size_t> split = idna::to_ascii(out, "a" + alone + "b");
        BOOST_TEST_EQ(std::string_view(out.data(), split.value()), "a.b");
    }
}

/** A name with several faults reports the first label's, then the Bidi rule, then the length. */
void the_first_error_wins() {
    buffer out{};
    BOOST_TEST(idna::to_ascii(out, "a_b.xn--zz").error() == idna::error::not_ldh);
    BOOST_TEST(idna::to_ascii(out, "xn--zz.a_b").error() == idna::error::invalid_punycode);
    BOOST_TEST(idna::to_ascii(out, "0a.\327\220.a_b").error() == idna::error::not_ldh);
    BOOST_TEST(idna::to_ascii(out, "0a.\327\220").error() == idna::error::bidi);
}

/** NUL and the other C0 controls are ASCII outside the STD3 rules, and NV8 in UTS 46's table. */
void control_characters() {
    buffer out{};
    const std::string_view with_nul("a\0b", 3);
    BOOST_TEST(idna::to_ascii(out, with_nul).error() == idna::error::not_ldh);
    BOOST_TEST(
        idna::to_ascii(out, with_nul, idna::options{.use_std3_ascii_rules = false}).error() ==
        idna::error::not_idna2008);
    const idna::result<std::size_t> loose = idna::to_ascii(
        out, with_nul, idna::options{.use_std3_ascii_rules = false, .idna2008 = false});
    BOOST_TEST(std::string_view(out.data(), loose.value()) == with_nul);
    const idna::unicode_result shown = idna::to_unicode(out, with_nul);
    BOOST_TEST(std::string_view(out.data(), shown.size) == with_nul);
    BOOST_TEST(shown.error == idna::error::not_ldh);
}

/** A final empty label is the DNS root: refused by ToASCII under VerifyDnsLength, kept by
    ToUnicode, and passed through by both without it. */
void the_root_label() {
    buffer out{};
    BOOST_TEST(idna::to_ascii(out, "example.").error() == idna::error::empty_label);
    const idna::unicode_result shown = idna::to_unicode(out, "example.");
    BOOST_TEST_EQ(std::string_view(out.data(), shown.size), "example.");
    BOOST_TEST(!shown.error);
    const idna::options no_lengths{.verify_dns_length = false};
    const idna::result<std::size_t> kept = idna::to_ascii(out, "example.", no_lengths);
    BOOST_TEST_EQ(std::string_view(out.data(), kept.value()), "example.");
    BOOST_TEST(idna::to_unicode(out, ".example").error == idna::error::empty_label);
}

/** The inline storage holds a label after canonical decomposition: 255 a and an e-acute decompose
    to 257 code points, beyond it, while their NFC is 256. */
void a_label_that_decomposes_past_the_storage() {
    const std::string label = std::string(255, 'a') + "\303\251";
    const idna::options no_lengths{.verify_dns_length = false};
    std::array<char, 400> out{};
    BOOST_TEST(idna::to_ascii(out, label, no_lengths).error() == idna::error::storage_exhausted);
    const idna::result<std::pmr::string> owned = idna::to_ascii(label, no_lengths);
    BOOST_TEST(owned.has_value());
}

/** The first CJK ideograph, U+4E00; every code point up to U+9FFF is one, valid in IDNA2008. */
constexpr char32_t first_ideograph = 0x4E00;

/** The UTF-8 of `text`. */
std::string utf8_of(std::u32string_view text) {
    std::string out;
    std::array<char, 4> octets{};
    for (const char32_t cp : text) {
        out.append(octets.data(), idna::detail::utf8::encode(cp, octets.data()));
    }
    return out;
}

/**
 `count` ideographs in `distinct` blocks of one each, in descending order, `count` a multiple of
 `distinct`: with as many blocks as ideographs, the worst case of Punycode's encoder, which passes
 over the label once per distinct code point; with fewer, still the worst of its decoder, which
 inserts each block before every one decoded before it; with one, a label Punycode handles in linear
 time both ways.
*/
std::u32string ideographs(std::size_t count, std::size_t distinct) {
    std::u32string text;
    for (std::size_t block = distinct; block > 0; --block) {
        text.append(count / distinct, first_ideograph + static_cast<char32_t>(block - 1));
    }
    return text;
}

/** The A-label of `text`. */
std::string a_label_of(std::u32string_view text) {
    const idna::result<std::pmr::string> encoded = idna::punycode::encode(text);
    return "xn--" + std::string(encoded.value());
}

/** Under verify_dns_length, the span and the pmr forms of to_ascii report the same first error: a
    label beyond the inline storage is label_too_long as it is gathered, before its own checks, and
    so is a decoded A-label whose NFC check's copy outgrows the storage. */
void both_forms_report_the_same_error() {
    std::u32string decomposing = U"-";
    decomposing.append(128, char32_t{0xE9});
    decomposing.push_back(0x301);
    const std::array<std::string, 3> names = {
        "a_b" + std::string(300, 'a'),
        "a_b." + std::string(300, 'a'),
        a_label_of(decomposing),
    };
    constexpr std::array<idna::error, 3> expected = {
        idna::error::label_too_long,
        idna::error::not_ldh,
        idna::error::label_too_long,
    };
    std::array<char, 400> out{};
    for (std::size_t i = 0; i < names.size(); ++i) {
        BOOST_TEST(idna::to_ascii(out, names.at(i)).error() == expected.at(i));
        BOOST_TEST(idna::to_ascii(names.at(i)).error() == expected.at(i));
    }
}

/** A label's Punycode holds at most detail::max_punycode_code_points code points in the pmr forms:
    the encoder's input in to_ascii, and the decoder's output in to_ascii and to_unicode; one more
    is punycode_overflow whatever options::ignore_invalid_punycode says, and the A-label is kept as
    it is in the Unicode form. to_unicode encodes nothing, so a label past the bound that is not an
    A-label is written as it is. */
void the_bound_on_punycode() {
    constexpr std::size_t bound = idna::detail::max_punycode_code_points;
    static_assert(bound == 256, "the Doc Comments of the pmr forms state it");
    const idna::options no_lengths{.verify_dns_length = false};
    const idna::options ignore{.verify_dns_length = false, .ignore_invalid_punycode = true};
    const std::u32string at_bound = ideographs(bound, bound);
    const std::u32string beyond = ideographs(bound + 1, bound + 1);

    const idna::result<std::pmr::string> encoded = idna::to_ascii(utf8_of(at_bound), no_lengths);
    BOOST_TEST(encoded.has_value() && std::string_view(encoded.value()) == a_label_of(at_bound));
    BOOST_TEST(idna::to_ascii(utf8_of(beyond), no_lengths).error() ==
               idna::error::punycode_overflow);
    const idna::result<std::pmr::string> kept = idna::to_ascii(a_label_of(at_bound), no_lengths);
    BOOST_TEST(kept.has_value() && std::string_view(kept.value()) == a_label_of(at_bound));
    BOOST_TEST(idna::to_ascii(a_label_of(beyond), no_lengths).error() ==
               idna::error::punycode_overflow);
    BOOST_TEST(idna::to_ascii(a_label_of(beyond), ignore).error() ==
               idna::error::punycode_overflow);

    const idna::unicode_string decoded = idna::to_unicode(a_label_of(at_bound));
    BOOST_TEST(!decoded.error);
    BOOST_TEST(std::string_view(decoded.value) == utf8_of(at_bound));
    for (const idna::options& opts : {idna::options(), ignore}) {
        const idna::unicode_string refused = idna::to_unicode(a_label_of(beyond), opts);
        BOOST_TEST(refused.error == idna::error::punycode_overflow);
        BOOST_TEST(std::string_view(refused.value) == a_label_of(beyond));
    }
    for (const idna::options& opts : {idna::options(), no_lengths}) {
        for (const std::u32string& text : {beyond, ideographs(300, 300)}) {
            const idna::unicode_string written = idna::to_unicode(utf8_of(text), opts);
            BOOST_TEST(!written.error);
            BOOST_TEST(std::string_view(written.value) == utf8_of(text));
        }
    }
}

/**
 The decoder stops at the bound, before the rest of the label: Punycode of 300 code points followed
 by a character that is no digit is punycode_overflow in both pmr forms, where a decoder that read
 the whole label, its quadratic work done, would find the character first, invalid_punycode.
*/
void the_decoder_stops_at_the_bound() {
    const std::string a_label = a_label_of(ideographs(300, 300)) + "!";
    const idna::options no_lengths{.verify_dns_length = false};
    BOOST_TEST(idna::to_unicode(a_label).error == idna::error::punycode_overflow);
    BOOST_TEST(idna::to_ascii(a_label, no_lengths).error() == idna::error::punycode_overflow);
    // Within the bound, the same character is found: the label is read to its end.
    BOOST_TEST(idna::to_unicode(a_label_of(ideographs(200, 200)) + "!").error ==
               idna::error::invalid_punycode);
}

/** The nanoseconds of the fastest of three runs of `convert`. */
template <class Convert>
long long fastest_of_three(Convert convert) {
    auto fastest = std::chrono::nanoseconds::max();
    for (int i = 0; i < 3; ++i) {
        const auto start = std::chrono::steady_clock::now();
        convert();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        fastest = std::min(fastest, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    return fastest.count();
}

/** The nanoseconds of the fastest of three runs of the pmr to_ascii over `domain` without the DNS
    lengths, each of which must give `expected`. */
long long to_ascii_time(const std::string& domain, const boost::system::error_code& expected) {
    return fastest_of_three([&] {
        const idna::options no_lengths{.verify_dns_length = false};
        const idna::result<std::pmr::string> found = idna::to_ascii(domain, no_lengths);
        BOOST_TEST(found.has_error() ? found.error() == expected : !expected);
    });
}

/** How many times slower than its baseline the timing test's input may be: refused, it stays near
    1 on every lane, and Punycode without the bound takes it past 29. */
constexpr long long slower = 10;

/** The code points of a label past the bound, distinct ideographs, so far below U+9FFF. */
constexpr std::size_t long_label = 16384;

static_assert(long_label > 4 * idna::detail::max_punycode_code_points);

/**
 A label of 16,384 distinct ideographs is refused in less than 10 times the time of a label of as
 many copies of one, the fastest of three runs each, in the pmr to_ascii: past the bound the encoder
 does no work, so the ratio stays near 1, where the unbounded encoder, which passes over the label
 once per distinct code point, takes it to 29 and more (165 at -O2).

 Tip: the decoder's bound is pinned by the_decoder_stops_at_the_bound, which no timing can make
 flaky.
*/
void the_encoder_beyond_the_bound_in_linear_time() {
    const boost::system::error_code overflow = idna::error::punycode_overflow;
    const long long baseline = to_ascii_time(utf8_of(ideographs(long_label, 1)), overflow);
    const long long measured = to_ascii_time(utf8_of(ideographs(long_label, long_label)), overflow);
    BOOST_TEST_LT(measured, slower * baseline);
}

}  // namespace

int main() {
    idna_test_v2();
    idna_test_v2_strict();
    the_length_comes_before_the_buffer();
    to_unicode_stops_at_a_code_point();
    to_unicode_goes_on();
    the_defaults();
    ignore_invalid_punycode();
    beyond_the_inline_storage();
    separators();
    the_first_error_wins();
    control_characters();
    the_root_label();
    a_label_that_decomposes_past_the_storage();
    both_forms_report_the_same_error();
    the_bound_on_punycode();
    the_decoder_stops_at_the_bound();
    the_encoder_beyond_the_bound_in_linear_time();
    return boost::report_errors();
}
