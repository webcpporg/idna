// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// Spot properties of the generated Unicode 17.0.0 tables, read through detail/properties.hpp: one
// value of each table that a wrong packing, a shifted run or a misread file would change.

#include <webcpp/idna/detail/properties.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string_view>

namespace detail = webcpp::idna::detail;
namespace tables = webcpp::idna::detail::tables;

namespace {

// The lookups are constant expressions, as the tables are.
static_assert(detail::uts46_of(U'A').mapping == U"a");
static_assert(detail::bidi_class_of(0x05D0) == tables::bidi_class::r);
static_assert(detail::combining_class_of(0x0300) == 230U);
static_assert(detail::decomposition_of(0x00E9).size() == 2U);
static_assert(detail::composition_of(U'e', 0x0301) == 0xE9);

void uts46_statuses_and_mappings() {
    // U+0000 opens every run list.
    const detail::uts46_entry nul = detail::uts46_of(0x0000);
    BOOST_TEST(nul.status == tables::status::valid);
    BOOST_TEST(nul.not_idna2008);
    BOOST_TEST(nul.mapping.empty());
    BOOST_TEST(detail::uts46_of(U'a').status == tables::status::valid);
    BOOST_TEST(!detail::uts46_of(U'a').not_idna2008);
    const detail::uts46_entry inverted_exclamation = detail::uts46_of(0x00A1);
    BOOST_TEST(inverted_exclamation.status == tables::status::valid);
    BOOST_TEST(inverted_exclamation.not_idna2008);
    const detail::uts46_entry capital_a = detail::uts46_of(U'A');
    BOOST_TEST(capital_a.status == tables::status::mapped);
    BOOST_TEST(capital_a.mapping == U"a");
    BOOST_TEST(detail::uts46_of(0x00AD).status == tables::status::ignored);
    const detail::uts46_entry sharp_s = detail::uts46_of(0x00DF);
    BOOST_TEST(sharp_s.status == tables::status::deviation);
    BOOST_TEST(sharp_s.mapping == U"ss");
    BOOST_TEST(detail::uts46_of(0x200D).status == tables::status::deviation);
    BOOST_TEST(detail::uts46_of(0x200D).mapping.empty());
    BOOST_TEST(detail::uts46_of(0x3002).mapping == U".");
    // The longest mapping of the table, U+FDFA, 18 code points.
    BOOST_TEST_EQ(detail::uts46_of(0xFDFA).mapping.size(), 18U);
    BOOST_TEST(detail::uts46_of(0xE000).status == tables::status::disallowed);
    BOOST_TEST(detail::uts46_of(0x10FFFF).status == tables::status::disallowed);
}

void bidi_classes() {
    BOOST_TEST(detail::bidi_class_of(0x0000) == tables::bidi_class::bn);
    BOOST_TEST(detail::bidi_class_of(U'a') == tables::bidi_class::l);
    BOOST_TEST(detail::bidi_class_of(0x05D0) == tables::bidi_class::r);
    BOOST_TEST(detail::bidi_class_of(0x0627) == tables::bidi_class::al);
    BOOST_TEST(detail::bidi_class_of(0x0660) == tables::bidi_class::an);
    BOOST_TEST(detail::bidi_class_of(U'0') == tables::bidi_class::en);
    BOOST_TEST(detail::bidi_class_of(0x0300) == tables::bidi_class::nsm);
    BOOST_TEST(detail::bidi_class_of(U' ') == tables::bidi_class::other);
    // An unassigned code point of the Hebrew block defaults to R (an @missing line).
    BOOST_TEST(detail::bidi_class_of(0x05FF) == tables::bidi_class::r);
}

void joining_types_and_combining_classes() {
    BOOST_TEST(detail::joining_type_of(0x0627) == tables::joining_type::r);
    BOOST_TEST(detail::joining_type_of(0x0628) == tables::joining_type::d);
    BOOST_TEST(detail::joining_type_of(0x0300) == tables::joining_type::t);
    BOOST_TEST(detail::joining_type_of(U'a') == tables::joining_type::other);
    BOOST_TEST_EQ(detail::combining_class_of(0x094D), 9U);
    BOOST_TEST_EQ(detail::combining_class_of(0x0300), 230U);
    BOOST_TEST_EQ(detail::combining_class_of(U'a'), 0U);
}

void scripts_and_marks() {
    BOOST_TEST(detail::script_of(0x0375) == tables::script::greek);
    BOOST_TEST(detail::script_of(0x03B2) == tables::script::greek);
    BOOST_TEST(detail::script_of(0x05D1) == tables::script::hebrew);
    BOOST_TEST(detail::script_of(0x3041) == tables::script::hiragana);
    BOOST_TEST(detail::script_of(0x30A2) == tables::script::katakana);
    BOOST_TEST(detail::script_of(0x30FB) == tables::script::other);  // KATAKANA MIDDLE DOT, Common
    BOOST_TEST(detail::script_of(0x4E00) == tables::script::han);
    BOOST_TEST(detail::is_mark(0x0300));
    BOOST_TEST(detail::is_mark(0x0903));  // Mc
    BOOST_TEST(!detail::is_mark(U'a'));
}

void normalization_data() {
    BOOST_TEST(detail::nfc_quick_check_of(U'a') == tables::quick_check::yes);
    BOOST_TEST(detail::nfc_quick_check_of(0x0340) == tables::quick_check::no);
    BOOST_TEST(detail::nfc_quick_check_of(0x0301) == tables::quick_check::maybe);
    BOOST_TEST(detail::decomposition_of(0x00E9) == U"e\u0301");
    // U+1FAF decomposes in full to four code points, as long as a canonical decomposition gets.
    BOOST_TEST_EQ(detail::decomposition_of(0x1FAF).size(), 4U);
    BOOST_TEST(detail::decomposition_of(U'a').empty());
    BOOST_TEST(detail::decomposition_of(0xAC00).empty());  // Hangul is algorithmic
    BOOST_TEST_EQ(static_cast<unsigned long>(detail::composition_of(U'e', 0x0301)), 0xE9UL);
    // U+0958 is a composition exclusion: its pair composes to nothing.
    BOOST_TEST_EQ(static_cast<unsigned long>(detail::composition_of(0x0915, 0x093C)), 0UL);
}

void the_version() {
    BOOST_TEST_EQ(tables::unicode_version, std::string_view("17.0.0"));
}

}  // namespace

int main() {
    uts46_statuses_and_mappings();
    bidi_classes();
    joining_types_and_combining_classes();
    scripts_and_marks();
    normalization_data();
    the_version();
    return boost::report_errors();
}
