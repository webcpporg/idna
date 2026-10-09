// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/idna/errors.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace idna = webcpp::idna;

namespace {

// The message into a caller's buffer throws nothing; the one that returns a std::string allocates
// it from the heap, and may throw what operator new throws (idna-invariant-2).
static_assert(noexcept(idna::error_category().message(0, nullptr, 0)));
static_assert(!noexcept(idna::error_category().message(0)));

// The call a program without exceptions makes, error_code's message into a caller's buffer, throws
// nothing for an error of idna (idna-invariant-2).
static_assert(noexcept(idna::make_error_code(idna::error::invalid_utf8)
                           .message(std::declval<char*>(), std::size_t{})));

/** Every enumerator with its fixed value and its message, as the guide's table spells them. */
constexpr auto spellings = std::to_array<std::pair<idna::error, std::string_view>>({
    {idna::error::invalid_utf8, "invalid UTF-8"},
    {idna::error::disallowed, "disallowed code point"},
    {idna::error::not_idna2008, "code point disallowed by IDNA2008"},
    {idna::error::not_ldh, "ASCII code point other than a letter, a digit or a hyphen"},
    {idna::error::not_nfc, "label not in Normalization Form C"},
    {idna::error::hyphen_in_third_and_fourth, "hyphens in the third and fourth positions"},
    {idna::error::hyphen_at_start_or_end, "hyphen at the start or end of a label"},
    {idna::error::ace_prefix, "label starts with xn--"},
    {idna::error::leading_combining_mark, "label starts with a combining mark"},
    {idna::error::contextj, "joiner outside its allowed context"},
    {idna::error::contexto, "code point outside its allowed context"},
    {idna::error::bidi, "Bidi rule violated"},
    {idna::error::invalid_punycode, "invalid Punycode"},
    {idna::error::punycode_overflow, "Punycode overflow"},
    {idna::error::empty_label, "empty label"},
    {idna::error::label_too_long, "label longer than 63 octets"},
    {idna::error::domain_too_long, "domain name longer than 253 octets"},
    {idna::error::output_too_small, "output too small"},
    {idna::error::storage_exhausted, "label beyond the working storage"},
});

void values_are_fixed() {
    int expected = 1;
    for (const auto& [e, message] : spellings) {
        BOOST_TEST_EQ(static_cast<int>(e), expected);
        ++expected;
    }
}

void the_category_is_webcpp_idna() {
    BOOST_TEST_EQ(std::string(idna::error_category().name()), "webcpp.idna");
    for (const auto& [e, message] : spellings) {
        const boost::system::error_code code = e;
        BOOST_TEST(code.category() == idna::error_category());
        BOOST_TEST_EQ(code.message(), message);
        BOOST_TEST(idna::make_error_code(e) == code);
    }
    BOOST_TEST_EQ(idna::error_category().message(0), "unknown webcpp.idna error");
    BOOST_TEST_EQ(idna::error_category().message(20), "unknown webcpp.idna error");
}

/** A category of the test's own whose identity is the one idna's must have: two categories with
    a non-zero identity compare by its value alone, so it equals idna's exactly when the values
    are the same, where a second object of idna's own class would equal it whatever the value. */
class identity_probe final : public boost::system::error_category {
public:
    constexpr identity_probe() noexcept : boost::system::error_category(0x69646E6100000000ULL) {}

    const char* name() const noexcept override { return "identity probe"; }

    std::string message(int /*value*/) const override { return {}; }
};

/** Two categories compare by their 64-bit identity, the ASCII of "idna" and four zero bytes. */
void the_identity_is_fixed() {
    const idna::detail::category other;
    BOOST_TEST(other == idna::error_category());
    const identity_probe probe;
    BOOST_TEST(probe == idna::error_category());
}

/** The message copied into a caller's buffer: cut to fit with its terminating zero, never written
    past the size given, and returned whole whatever the buffer. */
void a_message_fits_the_buffer_it_is_given() {
    const boost::system::error_category& category = idna::error_category();
    const int value = static_cast<int>(idna::error::invalid_punycode);
    constexpr std::string_view whole = "invalid Punycode";
    std::array<char, whole.size() + 2> buffer{};

    BOOST_TEST_EQ(std::string_view(category.message(value, nullptr, 8)), whole);

    buffer.fill('x');
    BOOST_TEST_EQ(std::string_view(category.message(value, buffer.data(), 0)), whole);
    BOOST_TEST_EQ(buffer[0], 'x');

    buffer.fill('x');
    BOOST_TEST_EQ(std::string_view(category.message(value, buffer.data(), 1)), whole);
    BOOST_TEST_EQ(buffer[0], '\0');
    BOOST_TEST_EQ(buffer[1], 'x');

    buffer.fill('x');
    BOOST_TEST_EQ(std::string_view(category.message(value, buffer.data(), 8)), whole);
    BOOST_TEST_EQ(std::string_view(buffer.data()), "invalid");
    BOOST_TEST_EQ(buffer[8], 'x');

    buffer.fill('x');
    BOOST_TEST_EQ(std::string_view(category.message(value, buffer.data(), whole.size() + 1)),
                  whole);
    BOOST_TEST_EQ(std::string_view(buffer.data()), whole);
    BOOST_TEST_EQ(buffer[whole.size() + 1], 'x');
}

void a_result_holds_an_error() {
    const idna::result<int> r = idna::error::invalid_utf8;
    BOOST_TEST(r.has_error());
    BOOST_TEST(r.error() == idna::error::invalid_utf8);
}

}  // namespace

int main() {
    values_are_fixed();
    the_category_is_webcpp_idna();
    the_identity_is_fixed();
    a_message_fits_the_buffer_it_is_given();
    a_result_holds_an_error();
    return boost::report_errors();
}
