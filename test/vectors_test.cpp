// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The generated test vectors hold every line of the pinned files, their blank fields resolved, as
// the files' headers say: a check of the generator's output that the conformance tests rest on.

#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "idna_vectors.hpp"
#include "nfc_vectors.hpp"
#include "require.hpp"

using webcpp::test::idna_vector;
using webcpp::test::idna_vectors;
using webcpp::test::nfc_vectors;

namespace {

void every_line_is_there() {
    BOOST_TEST_EQ(nfc_vectors.size(), 20034U);
    BOOST_TEST_EQ(idna_vectors.front().line, 106U);
    BOOST_TEST_EQ(idna_vectors.back().line, 6508U);
}

/** Line 107, "fa<U+00DF>.de; ; ; xn--fa-hia.de; ; fass.de;": blank fields take the value the
    header names. */
void blank_fields_are_resolved() {
    const idna_vector& line = idna_vectors[1];
    BOOST_TEST_EQ(line.line, 107U);
    BOOST_TEST_EQ(line.source, std::string_view("fa\303\237.de"));
    BOOST_TEST_EQ(line.to_unicode, line.source);
    BOOST_TEST(!line.to_unicode_error);
    BOOST_TEST_EQ(line.to_ascii_n, std::string_view("xn--fa-hia.de"));
    BOOST_TEST_EQ(line.to_ascii_t, std::string_view("fass.de"));
    BOOST_TEST(line.strict);
    BOOST_TEST(line.known_failure.empty());
}

/** The row of line `number` of IdnaTestV2.txt, or null after a failed check. */
const idna_vector* row_of(std::uint32_t number) {
    const auto at =
        std::ranges::find(idna_vectors, number, &idna_vector::line) - idna_vectors.begin();
    if (!BOOST_TEST(at < std::ssize(idna_vectors))) {
        return nullptr;
    }
    return &idna_vectors[static_cast<std::size_t>(at)];
}

/** Each other rule of the header, on a line whose blank field it alone resolves. */
void each_blank_field_takes_its_value() {
    // 109, "xn--fa-hia.de; fa<U+00DF>.de; ; xn--fa-hia.de; ; ;": toAsciiT is toAsciiN.
    if (const idna_vector* line = row_of(109)) {
        BOOST_TEST_EQ(line->to_unicode, std::string_view("fa\303\237.de"));
        BOOST_TEST_EQ(line->to_ascii_t, std::string_view("xn--fa-hia.de"));
    }
    // 153, "a<U+200C>b; ; [C1]; xn--ab-j1t; ; ab; []": an explicit [] is no error, though
    // toAsciiNStatus inherits [C1]; transitional processing drops the ZWNJ.
    if (const idna_vector* line = row_of(153)) {
        BOOST_TEST(line->to_unicode_error);
        BOOST_TEST(line->to_ascii_n_error);
        BOOST_TEST(!line->to_ascii_t_error);
        BOOST_TEST_EQ(line->to_ascii_t, std::string_view("ab"));
    }
    // 209, an A-label of 64 octets: toAsciiTStatus is toAsciiNStatus, [A4_2], not
    // toUnicodeStatus.
    if (const idna_vector* line = row_of(209)) {
        BOOST_TEST(!line->to_unicode_error);
        BOOST_TEST(line->to_ascii_n_error);
        BOOST_TEST(line->to_ascii_t_error);
    }
    // 543, "$; ; [U1]; ; ; ;": toAsciiN is toUnicode, and toAsciiNStatus toUnicodeStatus.
    if (const idna_vector* line = row_of(543)) {
        BOOST_TEST_EQ(line->to_ascii_n, std::string_view("$"));
        BOOST_TEST(line->to_unicode_error && line->to_ascii_n_error && line->to_ascii_t_error);
    }
    // 550, "xn--; \"\"; [P4, X4_2]; ; [P4, A4_1, A4_2]; ;": "" is the empty string, which a
    // blank toAsciiN then repeats.
    if (const idna_vector* line = row_of(550)) {
        BOOST_TEST_EQ(line->source, std::string_view("xn--"));
        BOOST_TEST(line->to_unicode.empty());
        BOOST_TEST(line->to_ascii_n.empty());
    }
}

/** Line 548, a then the lone surrogate U+D900 then z, which the generator writes as its three
    bytes, ill-formed UTF-8. */
void a_surrogate_is_ill_formed() {
    const idna_vector* line = row_of(548);
    if (line == nullptr) {
        return;
    }
    BOOST_TEST_EQ(line->source, std::string_view("a\355\244\200z"));
    BOOST_TEST(line->to_unicode_error && line->to_ascii_n_error && line->to_ascii_t_error);
}

/** The strict pass keeps the lines whose toUnicode field holds no NV8, XV8 or CONTEXTO code
    point: 3,084 of them in Unicode 17.0.0. */
void the_strict_lines() {
    const auto strict = std::ranges::count_if(idna_vectors, &idna_vector::strict);
    BOOST_TEST_EQ(strict, 3084);
}

void the_first_nfc_line() {
    BOOST_TEST(nfc_vectors.front().source == U"\u1E0A");
    BOOST_TEST(nfc_vectors.front().nfc == U"\u1E0A");
    BOOST_TEST(nfc_vectors.front().nfd == U"\u0044\u0307");
}

}  // namespace

int main() {
    // The cases below read lines by their index.
    webcpp::test::require(BOOST_TEST_EQ(idna_vectors.size(), 6391U));
    every_line_is_there();
    blank_fields_are_resolved();
    each_blank_field_takes_its_value();
    a_surrogate_is_ill_formed();
    the_strict_lines();
    the_first_nfc_line();
    return boost::report_errors();
}
