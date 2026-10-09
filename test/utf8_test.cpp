// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Strict UTF-8 decoding and encoding (RFC 3629 section 4's table of well-formed sequences), which
// every operation of idna runs its input through first.

#include <webcpp/idna/detail/utf8.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace utf8 = webcpp::idna::detail::utf8;

namespace {

/** Decodes the one code point `text` holds, which must take all of it, and encodes it back
    into the same octets. */
void decodes_to(std::string_view text, char32_t expected) {
    std::size_t i = 0;
    char32_t cp = 0;
    BOOST_TEST(utf8::decode(text, i, cp));
    BOOST_TEST_EQ(i, text.size());
    BOOST_TEST_EQ(static_cast<unsigned long>(cp), static_cast<unsigned long>(expected));
    BOOST_TEST(utf8::is_valid(text));
    std::array<char, 4> out{};
    BOOST_TEST_EQ(utf8::length(expected), text.size());
    BOOST_TEST_EQ(std::string_view(out.data(), utf8::encode(expected, out.data())), text);
}

/** Refuses `text` at its first byte, leaving the position where it was. */
void refuses(std::string_view text) {
    std::size_t i = 0;
    char32_t cp = 0x41;
    BOOST_TEST(!utf8::decode(text, i, cp));
    BOOST_TEST_EQ(i, 0U);
    BOOST_TEST_EQ(static_cast<unsigned long>(cp), 0x41UL);
    BOOST_TEST(!utf8::is_valid(text));
}

/** The boundaries of every row of RFC 3629's table, both ways. */
void well_formed_sequences() {
    decodes_to(std::string_view("\0", 1), 0x00);
    decodes_to("\177", 0x7F);
    decodes_to("\302\200", 0x80);
    decodes_to("\337\277", 0x7FF);
    decodes_to("\340\240\200", 0x800);
    decodes_to("\354\277\277", 0xCFFF);
    decodes_to("\355\200\200", 0xD000);
    decodes_to("\355\237\277", 0xD7FF);
    decodes_to("\356\200\200", 0xE000);
    decodes_to("\357\277\277", 0xFFFF);
    decodes_to("\360\220\200\200", 0x10000);
    decodes_to("\363\277\277\277", 0xFFFFF);
    decodes_to("\364\200\200\200", 0x100000);
    decodes_to("\364\217\277\277", 0x10FFFF);
}

void ill_formed_sequences() {
    refuses("\200");  // a lone continuation byte
    refuses("\277");
    refuses("\300\200");  // overlong, C0 and C1 never lead
    refuses("\301\277");
    refuses("\340\237\277");      // overlong three-byte form of U+07FF
    refuses("\360\217\277\277");  // overlong four-byte form of U+FFFF
    refuses("\355\240\200");      // the surrogate U+D800
    refuses("\355\277\277");      // the surrogate U+DFFF
    refuses("\364\220\200\200");  // U+110000
    refuses("\365\200\200\200");  // F5 to FF never lead
    refuses("\377");
    refuses("\303");  // truncated
    refuses("\342\202");
    refuses("\360\237\230");
    refuses("\303\050");  // a lead followed by no continuation
    // A bad continuation byte after a good one: a decoder that checks the second byte alone
    // would take it, the label separator "." among them, as part of the code point.
    refuses("\342\202(");
    refuses("\342\202.");
    refuses("\342\202\300");
    refuses("\360\237(\200");
    refuses("\360\237\230(");
    refuses("\360\237\230.");
    refuses("\364\217\277\300");
}

/** At the end of the text there is nothing to decode, and the position and the code point stay
    as they were. */
void nothing_left_to_decode() {
    std::size_t i = 0;
    char32_t cp = 0x41;
    BOOST_TEST(!utf8::decode("", i, cp));
    BOOST_TEST_EQ(i, 0U);
    i = 1;
    BOOST_TEST(!utf8::decode("b", i, cp));
    BOOST_TEST_EQ(i, 1U);
    BOOST_TEST_EQ(static_cast<unsigned long>(cp), 0x41UL);
}

/** A string whose first sequence is good and second is not is refused as a whole. */
void a_string_is_valid_only_whole() {
    BOOST_TEST(utf8::is_valid(""));
    BOOST_TEST(utf8::is_valid("b\303\274cher"));
    BOOST_TEST(!utf8::is_valid("b\303\274\303"));
}

void encodes_every_length() {
    std::array<char, 4> out{};
    BOOST_TEST_EQ(utf8::encode(0x41, out.data()), 1U);
    BOOST_TEST_EQ(std::string_view(out.data(), 1), "A");
    BOOST_TEST_EQ(utf8::encode(0xFC, out.data()), 2U);
    BOOST_TEST_EQ(std::string_view(out.data(), 2), "\303\274");
    BOOST_TEST_EQ(utf8::encode(0x20AC, out.data()), 3U);
    BOOST_TEST_EQ(std::string_view(out.data(), 3), "\342\202\254");
    BOOST_TEST_EQ(utf8::encode(0x1F600, out.data()), 4U);
    BOOST_TEST_EQ(std::string_view(out.data(), 4), "\360\237\230\200");
    BOOST_TEST_EQ(utf8::length(0x7F), 1U);
    BOOST_TEST_EQ(utf8::length(0x7FF), 2U);
    BOOST_TEST_EQ(utf8::length(0xFFFF), 3U);
    BOOST_TEST_EQ(utf8::length(0x10FFFF), 4U);
}

}  // namespace

int main() {
    well_formed_sequences();
    ill_formed_sequences();
    nothing_left_to_decode();
    a_string_is_valid_only_whole();
    encodes_every_length();
    return boost::report_errors();
}
