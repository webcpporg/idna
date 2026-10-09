// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// IDNA2008's validity of labels and domains as they are given: every rule with the error it
// reports, and which of two broken rules is reported; the strings the JSON Schema suite's
// idn-hostname finds every UTS 46 library wrong on, as U-labels and as A-labels; the DNS lengths at
// their limits; a name of many labels checked in linear time; and the rows of IdnaTestV2.txt whose
// outputs are names as given.

#include <webcpp/idna/validity.hpp>

#include "idna_vectors.hpp"
#include "known_failures.hpp"

#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

namespace idna = webcpp::idna;

namespace {

// The validators throw nothing (idna-invariant-2).
static_assert(noexcept(idna::check_label(std::string_view())));
static_assert(noexcept(idna::check_domain(std::string_view())));
static_assert(noexcept(idna::is_valid_label(std::string_view())));
static_assert(noexcept(idna::is_valid_domain(std::string_view())));

/** check_domain refuses `domain` with `expected`, and is_valid_domain agrees. */
void refuses(std::string_view domain, idna::error expected,
             const idna::options& opts = idna::options()) {
    const idna::result<void> found = idna::check_domain(domain, opts);
    if (BOOST_TEST(found.has_error())) {
        BOOST_TEST_EQ(found.error(), idna::make_error_code(expected));
    }
    BOOST_TEST(!idna::is_valid_domain(domain, opts));
}

/** check_domain accepts `domain`, and is_valid_domain agrees. */
void accepts(std::string_view domain, const idna::options& opts = idna::options()) {
    const idna::result<void> found = idna::check_domain(domain, opts);
    if (!BOOST_TEST(found.has_value())) {
        BOOST_TEST_EQ(found.error(), boost::system::error_code());
    }
    BOOST_TEST(idna::is_valid_domain(domain, opts));
}

/** The 15 invalid idn-hostname strings of the JSON Schema suite that UTS 46 alone accepts, as
    U-labels, and the 13 A-labels of its hostname tests. */
void what_uts46_alone_accepts() {
    refuses("\354\213\244\343\200\256\353\241\200.\355\205\214\354\212\244\355\212\270",
            idna::error::not_idna2008);                      // U+302E
    refuses("\331\200\337\272", idna::error::not_idna2008);  // U+0640 U+07FA
    refuses(
        "\343\200\261\343\200\262\343\200\263\343\200\264\343\200\265\343\200\256\343\200\257\343"
        "\200\273",
        idna::error::not_idna2008);
    refuses("a\302\267l", idna::error::contexto);
    refuses("\302\267l", idna::error::contexto);
    refuses("l\302\267a", idna::error::contexto);
    refuses("l\302\267", idna::error::contexto);
    // The suite's "alpha, keraia, S", its S lowercase: as given, a capital letter is disallowed
    // first.
    refuses("\316\261\315\265s", idna::error::contexto);
    refuses("\316\261\315\265", idna::error::contexto);
    refuses("\327\263\327\221", idna::error::contexto);  // geresh first
    refuses("\327\264\327\221", idna::error::contexto);  // gershayim first
    refuses("def\343\203\273abc", idna::error::contexto);
    refuses("\343\203\273", idna::error::contexto);
    refuses("0a.\327\220", idna::error::bidi);
    refuses("xn--7a", idna::error::not_idna2008);  // U+00A1
    for (const std::string_view a_label :
         {"xn--07jt112bpxg.xn--9t4b11yi5a", "xn--chb89f", "xn--07jceefgh4c"}) {
        refuses(a_label, idna::error::not_idna2008);
    }
    for (const std::string_view a_label : {
             "xn--al-0ea",
             "xn--l-fda",
             "xn--la-0ea",
             "xn--l-gda",
             "xn--s-jib3p",
             "xn--wva3j",
             "xn--5db1e",
             "xn--5db3e",
             "xn--defabc-k64e",
             "xn--vek",
         }) {
        refuses(a_label, idna::error::contexto);
    }
    refuses("0a.xn--4db", idna::error::bidi);
}

/** Their valid neighbours, and the same strings under UTS 46 alone. */
void what_idna2008_accepts() {
    accepts("l\302\267l");
    accepts("\316\261\315\265\316\262");
    accepts("\327\221\327\263");
    accepts("\343\203\273\343\201\201");
    accepts("a.\327\220");
    accepts("0a.b");  // no RTL label: not a Bidi domain name
    accepts("xn--bcher-kva.example");
    accepts("b\303\274cher.example");
    const idna::options uts46_alone{.idna2008 = false};
    accepts("a\302\267l", uts46_alone);
    accepts("xn--7a", uts46_alone);
    accepts("0a.\327\220", idna::options{.check_bidi = false});
}

/** An A-label is "xn--" in any ASCII case, decoded, validated and re-encoded. */
void a_labels() {
    BOOST_TEST(idna::is_valid_label("xn--bcher-kva"));
    BOOST_TEST(idna::is_valid_label("XN--BCHER-KVA"));
    BOOST_TEST(idna::check_label("xn--").error() == idna::error::invalid_punycode);
    BOOST_TEST(idna::check_label("xn--abc-").error() == idna::error::invalid_punycode);
    BOOST_TEST(idna::check_label("xn---9uc").error() == idna::error::invalid_punycode);
    BOOST_TEST(idna::check_label("xn--zz").error() == idna::error::invalid_punycode);
    BOOST_TEST(idna::check_label("xn--b\303\274cher").error() == idna::error::invalid_punycode);
    // U+0161 ends in the byte of "a": read as ASCII, the label would be "xn--bcher-kva".
    BOOST_TEST(idna::check_label("xn--bcher-kv\305\241").error() == idna::error::invalid_punycode);
    BOOST_TEST(idna::check_label("xn--ls8h").error() == idna::error::not_idna2008);  // U+1F4A9
    BOOST_TEST(idna::is_valid_label("xn--ls8h", idna::options{.idna2008 = false}));
    // A decoded label that starts with "xn--": V2 with CheckHyphens, V4 without.
    BOOST_TEST(idna::check_label("xn--xn---3ra").error() ==
               idna::error::hyphen_in_third_and_fourth);
    BOOST_TEST(idna::check_label("xn--xn---3ra", idna::options{.check_hyphens = false}).error() ==
               idna::error::ace_prefix);
}

/** NR-LDH labels, R-LDH labels, and every rule of a U-label as it is given, nothing mapped. */
void labels_as_given() {
    BOOST_TEST(idna::is_valid_label("example"));
    BOOST_TEST(idna::check_label("ab--cd").error() == idna::error::hyphen_in_third_and_fourth);
    BOOST_TEST(idna::is_valid_label("ab--cd", idna::options{.check_hyphens = false}));
    BOOST_TEST(idna::check_label("-ab").error() == idna::error::hyphen_at_start_or_end);
    BOOST_TEST(idna::check_label("ab-").error() == idna::error::hyphen_at_start_or_end);
    BOOST_TEST(idna::check_label("Example").error() == idna::error::disallowed);
    BOOST_TEST(idna::check_label("e\314\201").error() == idna::error::not_nfc);
    BOOST_TEST(idna::check_label("\314\201e").error() == idna::error::leading_combining_mark);
    BOOST_TEST(idna::check_label("a_b").error() == idna::error::not_ldh);
    const idna::options no_std3{.use_std3_ascii_rules = false};
    BOOST_TEST(idna::check_label("a_b", no_std3).error() == idna::error::not_idna2008);
    BOOST_TEST(idna::is_valid_label(
        "a_b", idna::options{.use_std3_ascii_rules = false, .idna2008 = false}));
    BOOST_TEST(idna::check_label("a\342\200\214b").error() == idna::error::contextj);
    BOOST_TEST(idna::is_valid_label("a\342\200\214b", idna::options{.check_joiners = false}));
    BOOST_TEST(idna::check_label("a.b").error() == idna::error::disallowed);
    BOOST_TEST(idna::check_label("\303").error() == idna::error::invalid_utf8);
    BOOST_TEST(idna::check_label("0\327\220").error() == idna::error::bidi);
}

/** The rules in the order of the guide: of two broken rules, the earlier one is the error. */
void the_order_of_the_rules() {
    BOOST_TEST(idna::check_label("-e\314\201").error() == idna::error::not_nfc);
    BOOST_TEST(idna::check_label("\314\201a--b").error() ==
               idna::error::hyphen_in_third_and_fourth);
    BOOST_TEST(idna::check_label("\314\201ab-").error() == idna::error::hyphen_at_start_or_end);
    BOOST_TEST(idna::check_label("\314\201A").error() == idna::error::leading_combining_mark);
    BOOST_TEST(idna::check_label("A_").error() == idna::error::disallowed);
    BOOST_TEST(idna::check_label("a_b\342\200\214").error() == idna::error::not_ldh);
    BOOST_TEST(idna::check_label("a\342\200\214\302\241").error() == idna::error::contextj);
    BOOST_TEST(idna::check_label("\302\241\302\267").error() == idna::error::not_idna2008);
    BOOST_TEST(idna::check_label("a.\303").error() == idna::error::invalid_utf8);
    // A decoded A-label is held to NFC too: "xn--e-xbb" is e and a combining acute.
    BOOST_TEST(idna::check_label("xn--e-xbb").error() == idna::error::not_nfc);
}

/** A deviation, sharp s, is valid as given in nontransitional processing alone; an A-label's
    U-label is always held to nontransitional processing. */
void deviations() {
    const idna::options transitional{.transitional_processing = true};
    BOOST_TEST(idna::is_valid_label("fa\303\237"));
    BOOST_TEST(idna::check_label("fa\303\237", transitional).error() == idna::error::disallowed);
    BOOST_TEST(idna::is_valid_label("xn--fa-hia"));
    BOOST_TEST(idna::is_valid_label("xn--fa-hia", transitional));
}

/** Labels of 63 and 64 octets in their ASCII form, names of 253 and 254, and empty labels. */
void lengths() {
    const std::string label_63(63, 'a');
    BOOST_TEST(idna::is_valid_label(label_63));
    BOOST_TEST(idna::check_label(label_63 + "a").error() == idna::error::label_too_long);
    // 55 a and a u-umlaut make an A-label of 63 octets; 56 make one of 64.
    BOOST_TEST(idna::is_valid_label(std::string(55, 'a') + "\303\274"));
    BOOST_TEST(idna::check_label(std::string(56, 'a') + "\303\274").error() ==
               idna::error::label_too_long);
    // The same two labels as A-labels, given as such.
    BOOST_TEST(idna::is_valid_label("xn--" + std::string(55, 'a') + "-8yf"));
    BOOST_TEST(idna::check_label("xn--" + std::string(56, 'a') + "-t2f").error() ==
               idna::error::label_too_long);
    const std::string name_253 =
        label_63 + "." + label_63 + "." + label_63 + "." + std::string(61, 'a');
    BOOST_TEST_EQ(name_253.size(), 253U);
    accepts(name_253);
    refuses(name_253 + "a", idna::error::domain_too_long);
    // The same lengths with A-labels given as such, separators counted before each.
    const std::string a_label_63 = "xn--" + std::string(55, 'a') + "-8yf";
    const std::string a_name_253 =
        a_label_63 + "." + a_label_63 + "." + a_label_63 + "." + std::string(61, 'a');
    accepts(a_name_253);
    refuses(a_name_253 + "a", idna::error::domain_too_long);
    refuses("", idna::error::empty_label);
    refuses("a..b", idna::error::empty_label);
    refuses("a.", idna::error::empty_label);  // the root label, under VerifyDnsLength
    accepts("a.", idna::options{.verify_dns_length = false});
    accepts("a..b", idna::options{.verify_dns_length = false});
}

/** A label beyond the inline storage is too long for the DNS; without its lengths, it is
    storage_exhausted. */
void beyond_the_inline_storage() {
    const std::string long_label(300, 'a');
    const idna::options no_lengths{.verify_dns_length = false};
    BOOST_TEST(idna::check_label(long_label).error() == idna::error::label_too_long);
    BOOST_TEST(idna::check_label(long_label, no_lengths).error() == idna::error::storage_exhausted);
    // 128 e-acute and a combining acute fit as given, but the NFC check's scratch copy holds
    // their decomposition, 257 code points: the same two errors.
    std::string slow;
    for (int i = 0; i < 128; ++i) {
        slow += "\303\251";
    }
    slow += "\314\201";
    BOOST_TEST(idna::check_label(slow).error() == idna::error::label_too_long);
    BOOST_TEST(idna::check_label(slow, no_lengths).error() == idna::error::storage_exhausted);
}

/** Only U+002E separates labels as they are given: U+FF0E is a code point UTS 46 maps, so it is
    disallowed. */
void separators_as_given() {
    BOOST_TEST(idna::check_domain("a\357\274\216b").error() == idna::error::disallowed);
    BOOST_TEST(idna::check_domain("a\343\200\202b").error() == idna::error::disallowed);
    accepts("a.b");
}

/** NUL inside a label: ASCII outside the STD3 rules, and NV8 in UTS 46's table. */
void control_characters_as_given() {
    const std::string_view with_nul("a\0b", 3);
    BOOST_TEST(idna::check_label(with_nul).error() == idna::error::not_ldh);
    BOOST_TEST(idna::check_label(with_nul, idna::options{.use_std3_ascii_rules = false}).error() ==
               idna::error::not_idna2008);
    BOOST_TEST(idna::is_valid_label(
        with_nul, idna::options{.use_std3_ascii_rules = false, .idna2008 = false}));
}

/** The inline storage holds 256 code points of a label as given, and refuses the 257th. */
void the_inline_storage_edge() {
    const idna::options no_lengths{.verify_dns_length = false};
    BOOST_TEST(idna::is_valid_label(std::string(256, 'a'), no_lengths));
    BOOST_TEST(idna::check_label(std::string(257, 'a'), no_lengths).error() ==
               idna::error::storage_exhausted);
}

/** A label whose first code points end in the bytes of "xn" is no A-label: only ASCII letters
    are read in either case. U+0278 and U+026E end in 0x78 and 0x6E. */
void labels_that_only_look_like_a_labels() {
    const std::string_view look_alike = "\311\270\311\256--a";
    const idna::options no_hyphens{.check_hyphens = false};
    BOOST_TEST(idna::check_label(look_alike).error() == idna::error::hyphen_in_third_and_fourth);
    BOOST_TEST(idna::is_valid_label(look_alike, no_hyphens));
    // The same label as an A-label: decoded, it does not start with "xn--" either (V4).
    BOOST_TEST(idna::check_label("xn----a-qvb3c").error() ==
               idna::error::hyphen_in_third_and_fourth);
    BOOST_TEST(idna::is_valid_label("xn----a-qvb3c", no_hyphens));
}

/** `count` labels of `label`, separated by dots. */
std::string repeated(std::string_view label, std::size_t count) {
    std::string domain;
    for (std::size_t i = 0; i < count; ++i) {
        domain.append(label).append(".");
    }
    domain.pop_back();
    return domain;
}

/** A name with several faults reports its first label's, before the Bidi rule and the name's
    length; a label longer than the working storage is refused before its rules. */
void the_first_error_wins() {
    refuses("a_b.xn--zz", idna::error::not_ldh);
    refuses("xn--zz.a_b", idna::error::invalid_punycode);
    refuses("0a.\327\220.a_b", idna::error::not_ldh);
    refuses("a_b." + std::string(300, 'a'), idna::error::not_ldh);
    const std::string four_labels_63 = repeated(std::string(63, 'a'), 4);
    refuses("0a.\327\220." + four_labels_63, idna::error::bidi);
    refuses("0a.\327\220." + std::string(64, 'a'), idna::error::label_too_long);
    BOOST_TEST(idna::check_label("a_" + std::string(62, 'a')).error() == idna::error::not_ldh);
    BOOST_TEST(idna::check_label("a_" + std::string(300, 'a')).error() ==
               idna::error::label_too_long);
    refuses("0a.\327\220." + four_labels_63, idna::error::domain_too_long,
            idna::options{.check_bidi = false});
    refuses("a_b." + std::string(300, 'a'), idna::error::not_ldh,
            idna::options{.verify_dns_length = false});
}

/** The nanoseconds of the fastest of three runs of check_domain over `domain`, each of which must
    succeed. */
long long fastest_of_three(const std::string& domain, const idna::options& opts) {
    auto fastest = std::chrono::nanoseconds::max();
    for (int i = 0; i < 3; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const bool valid = idna::is_valid_domain(domain, opts);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        BOOST_TEST(valid);
        fastest = std::min(fastest, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    return fastest.count();
}

/**
 A name of 65,536 one-letter labels is checked in less than 20 times the time of a name as long of
 labels of 255 letters, the fastest of three runs each: the work per label is bounded, so the ratio
 stays near 1 on every lane, and a pass over the name per label takes it past 100.
*/
void many_labels_in_linear_time() {
    const idna::options no_lengths{.verify_dns_length = false};
    constexpr std::size_t long_labels = 512;
    const std::string baseline_domain = repeated(std::string(255, 'a'), long_labels);
    const std::string measured_domain = repeated("a", long_labels * 128);
    BOOST_TEST_EQ(baseline_domain.size(), measured_domain.size());
    const long long baseline = fastest_of_three(baseline_domain, no_lengths);
    const long long measured = fastest_of_three(measured_domain, no_lengths);
    BOOST_TEST_LT(measured, 20 * baseline);
}

/** Whether `domain` holds an empty label, which no row's output may be checked for as given: its
    verdict is the DNS lengths', and toUnicode's X4_2 applies to it alone. */
bool has_empty_label(std::string_view domain) {
    return domain.empty() || domain.front() == '.' || domain.back() == '.' ||
           domain.find("..") != std::string_view::npos;
}

/** `domain` with every letter of each label that starts with "xn--" in uppercase. */
std::string upper_a_labels(std::string_view domain) {
    std::string upper(domain);
    std::size_t begin = 0;
    while (begin <= upper.size()) {
        const std::size_t end = std::min(upper.find('.', begin), upper.size());
        if (std::string_view(upper).substr(begin, end - begin).starts_with("xn--")) {
            for (std::size_t i = begin; i < end; ++i) {
                upper[i] = upper[i] >= 'a' && upper[i] <= 'z' ? static_cast<char>(upper[i] - 32)
                                                              : upper[i];
            }
        }
        begin = end + 1;
    }
    return upper;
}

/** How many checks of each kind a pass over IdnaTestV2.txt made. */
struct as_given_counts {
    /** Error-free toAsciiN results checked. */
    std::size_t ascii_n = 0;

    /** Error-free toAsciiT results checked. */
    std::size_t ascii_t = 0;

    /** Error-free toUnicode results checked. */
    std::size_t unicode = 0;

    /** Sources already in their processed form checked. */
    std::size_t source = 0;

    /** Sources already in their ASCII form checked. */
    std::size_t ascii_source = 0;

    /** Names of one label also checked by check_label. */
    std::size_t labels = 0;
};

/** The verdict of check_domain on `domain`, which check_label must share when `domain` is one
    label. */
bool valid_as_given(std::string_view domain, const idna::options& opts, as_given_counts& counts) {
    const bool valid = idna::is_valid_domain(domain, opts);
    if (domain.find('.') == std::string_view::npos) {
        ++counts.labels;
        BOOST_TEST_EQ(idna::is_valid_label(domain, opts), valid);
    }
    return valid;
}

/**
 The checks of one row as given, and whether every one held: an error-free toAsciiN is valid, its
 A-labels in uppercase too, and so is an error-free toAsciiT; without the DNS lengths, an error-free
 toUnicode is valid, and a source already in its processed form has toUnicode's verdict; a source
 already in its ASCII form has toAsciiN's.
*/
bool row_as_given(const webcpp::test::idna_vector& row, const idna::options& opts,
                  as_given_counts& counts) {
    idna::options no_lengths = opts;
    no_lengths.verify_dns_length = false;
    bool held = true;
    if (!row.to_ascii_n_error) {
        ++counts.ascii_n;
        held = valid_as_given(row.to_ascii_n, opts, counts) && held;
        held = valid_as_given(upper_a_labels(row.to_ascii_n), opts, counts) && held;
    }
    if (!row.to_ascii_t_error) {
        ++counts.ascii_t;
        held = valid_as_given(row.to_ascii_t, opts, counts) && held;
    }
    if (!row.to_unicode_error && !has_empty_label(row.to_unicode)) {
        ++counts.unicode;
        held = valid_as_given(row.to_unicode, no_lengths, counts) && held;
    }
    const bool processed = row.source == row.to_unicode && !has_empty_label(row.source) &&
                           webcpp::idna::detail::utf8::is_valid(row.source);
    if (processed) {
        ++counts.source;
        held = valid_as_given(row.source, no_lengths, counts) == !row.to_unicode_error && held;
    }
    if (row.source == row.to_ascii_n) {
        ++counts.ascii_source;
        held = valid_as_given(row.source, opts, counts) == !row.to_ascii_n_error && held;
    }
    return held;
}

/**
 IdnaTestV2.txt's rows through check_domain and check_label, as given: UTS 46 alone over every row,
 then the strict profile over the rows of the strict pass, each row's verdict held against
 test/known_failures.json.

 Tip: UTS 46's mapping of a row's processed form changes nothing, so each output the file records
 as error-free must be valid as given, and a source already in that form has its verdict.
*/
void idna_test_v2_as_given() {
    for (const bool strict : {false, true}) {
        const idna::options opts{.idna2008 = strict};
        as_given_counts counts;
        for (const webcpp::test::idna_vector& row : webcpp::test::idna_vectors) {
            if (strict && !row.strict) {
                continue;
            }
            webcpp::test::as_listed(row.line, row.known_failure, row_as_given(row, opts, counts));
        }
        BOOST_TEST_GT(counts.ascii_n, 0U);
        BOOST_TEST_GT(counts.ascii_t, 0U);
        BOOST_TEST_GT(counts.unicode, 0U);
        BOOST_TEST_GT(counts.source, 0U);
        BOOST_TEST_GT(counts.ascii_source, 0U);
        BOOST_TEST_GT(counts.labels, 0U);
    }
}

}  // namespace

int main() {
    what_uts46_alone_accepts();
    what_idna2008_accepts();
    a_labels();
    labels_as_given();
    the_order_of_the_rules();
    deviations();
    lengths();
    beyond_the_inline_storage();
    separators_as_given();
    control_characters_as_given();
    the_inline_storage_edge();
    labels_that_only_look_like_a_labels();
    the_first_error_wins();
    many_labels_in_linear_time();
    idna_test_v2_as_given();
    return boost::report_errors();
}
