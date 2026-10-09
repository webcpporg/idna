// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The label driver in both forms, on the inputs that end a label early: an A-label that decodes to
// nothing, a label beyond the inline storage after an earlier error, gathered or in the NFC check's
// copy, and the empty labels; and what the driver may throw. test/Jamfile builds it with the
// standard library's assertions on, so that a read past an empty label traps.

#include <webcpp/idna/detail/process.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>

namespace detail = webcpp::idna::detail;
namespace idna = webcpp::idna;

namespace {

/** The driver over an inline label into a span or nothing can throw nothing; over a growing label,
    or into a string, it may throw what the memory resource throws (idna-invariant-2). */
template <class Label, class Sink>
constexpr bool nothrow = noexcept(
    detail::process(std::string_view(), std::declval<const idna::options&>(), detail::mode::map,
                    detail::form::ascii, std::declval<Label&>(), std::declval<Sink&>()));

static_assert(nothrow<detail::inline_label, detail::span_sink>);
static_assert(nothrow<detail::inline_label, detail::discard_sink>);
static_assert(!nothrow<detail::growing_label, detail::span_sink>);
static_assert(!nothrow<detail::inline_label, detail::string_sink>);

/** Runs the driver over `domain` in `f`, and returns what it wrote and its first error. */
std::pair<std::string, boost::system::error_code> run(std::string_view domain, detail::mode m,
                                                      detail::form f,
                                                      const idna::options& opts = idna::options()) {
    std::array<char, 1024> out{};
    detail::inline_label label;
    detail::span_sink sink(out);
    const boost::system::error_code found = detail::process(domain, opts, m, f, label, sink);
    return {std::string(out.data(), sink.written()), found};
}

/** "xn--" decodes to an empty label: invalid_punycode, and nothing is read of it (UTS 46 P4,
    IdnaTestV2.txt line 550). */
void an_a_label_that_decodes_to_nothing() {
    for (const detail::form f : {detail::form::ascii, detail::form::unicode}) {
        const auto [written, found] = run("xn--", detail::mode::map, f);
        BOOST_TEST(found == idna::error::invalid_punycode);
        BOOST_TEST(written.empty());
    }
    const auto [written, found] = run("a.xn--.b", detail::mode::map, detail::form::unicode);
    BOOST_TEST(found == idna::error::invalid_punycode);
    BOOST_TEST_EQ(written, "a..b");
    BOOST_TEST(run("xn--", detail::mode::as_given, detail::form::ascii).second ==
               idna::error::invalid_punycode);
}

/** A label beyond the storage stops the driver, but the Unicode form keeps the error recorded
    before it, and writes no more after the last label it ended. */
void the_first_error_survives_the_storage() {
    const std::string domain = "a_b." + std::string(300, 'a');
    const auto [written, found] = run(domain, detail::mode::map, detail::form::unicode);
    BOOST_TEST(found == idna::error::not_ldh);
    BOOST_TEST_EQ(written, "a_b");
}

/** The NFC check's copy of a decoded A-label beyond the storage stops the driver as a label
    beyond it does: 128 e-acute and a combining acute decompose to 257 code points. */
void the_nfc_copy_stops_the_driver_too() {
    const std::string a_label = "xn--9ca" + std::string(127, 'a') + "8520e";
    const auto [written, found] =
        run("a_b." + a_label + ".c", detail::mode::map, detail::form::unicode);
    BOOST_TEST(found == idna::error::not_ldh);
    BOOST_TEST_EQ(written, "a_b");
    BOOST_TEST(run(a_label, detail::mode::map, detail::form::unicode).second ==
               idna::error::storage_exhausted);
    BOOST_TEST(run(a_label, detail::mode::map, detail::form::ascii).second ==
               idna::error::label_too_long);
}

/** A growing label holds what the inline storage cannot, from its memory resource: the same
    A-label is valid, and written whole. */
void a_growing_label_goes_on() {
    std::pmr::monotonic_buffer_resource resource;
    std::pmr::string out(&resource);
    detail::growing_label label(resource);
    detail::string_sink sink(out);
    const std::string a_label = "xn--9ca" + std::string(127, 'a') + "8520e";
    const idna::options no_lengths{.verify_dns_length = false};
    const boost::system::error_code found = detail::process(
        "a." + a_label, no_lengths, detail::mode::map, detail::form::unicode, label, sink);
    BOOST_TEST(!found);
    BOOST_TEST_EQ(out.size(), 2 + (128 * 2) + 2);
}

/** An empty label is an error under the DNS lengths, but for the root label, a final empty one
    after another, in the Unicode form (UTS 46 X4_2). */
void the_root_label() {
    for (const detail::form f : {detail::form::ascii, detail::form::unicode}) {
        BOOST_TEST(run("", detail::mode::map, f).second == idna::error::empty_label);
        BOOST_TEST(run(".a", detail::mode::map, f).second == idna::error::empty_label);
        BOOST_TEST(run("a..b", detail::mode::map, f).second == idna::error::empty_label);
    }
    BOOST_TEST(run("a.", detail::mode::map, detail::form::ascii).second ==
               idna::error::empty_label);
    const auto [written, found] = run("a.", detail::mode::map, detail::form::unicode);
    BOOST_TEST(!found);
    BOOST_TEST_EQ(written, "a.");
    BOOST_TEST(run("a..", detail::mode::map, detail::form::unicode).second ==
               idna::error::empty_label);
    const idna::options no_lengths{.verify_dns_length = false};
    BOOST_TEST(!run("", detail::mode::map, detail::form::ascii, no_lengths).second);
    BOOST_TEST_EQ(run("a..b", detail::mode::map, detail::form::ascii, no_lengths).first, "a..b");
}

/** An A-label whose Punycode does not decode is kept unchecked under ignore_invalid_punycode, as it
    is: the ASCII form holds it to the DNS lengths, and the Unicode form has none (UTS 46 4.3). */
void undecodable_labels_kept() {
    const idna::options ignore{.ignore_invalid_punycode = true};
    const std::string undecodable = "xn--_" + std::string(70, '9');
    BOOST_TEST(run(undecodable, detail::mode::map, detail::form::ascii).second ==
               idna::error::invalid_punycode);
    BOOST_TEST(run(undecodable, detail::mode::map, detail::form::ascii, ignore).second ==
               idna::error::label_too_long);
    const auto [written, found] =
        run("a." + undecodable, detail::mode::map, detail::form::unicode, ignore);
    BOOST_TEST(!found);
    BOOST_TEST_EQ(written, "a." + undecodable);
    BOOST_TEST(!run("xn--_99", detail::mode::map, detail::form::ascii, ignore).second);
    // A kept label is unchecked, the Bidi rule included: with U+05D0 before it, the name is a Bidi
    // domain name, where a label that ends with "_" would break rule 6.
    for (const detail::form f : {detail::form::ascii, detail::form::unicode}) {
        BOOST_TEST(!run("\327\220.xn--_", detail::mode::map, f, ignore).second);
    }
    const idna::options lax{.use_std3_ascii_rules = false, .idna2008 = false};
    BOOST_TEST(run("\327\220.a_", detail::mode::as_given, detail::form::ascii, lax).second ==
               idna::error::bidi);
}

/** As given, the ASCII form of an A-label is the label itself, its letters in lowercase, and of a
    U-label its A-label. */
void the_ascii_form_as_given() {
    const auto [written, found] = run("xn--bcher-kva.XN--BCHER-KVA.b\303\274cher.a",
                                      detail::mode::as_given, detail::form::ascii);
    BOOST_TEST(!found);
    BOOST_TEST_EQ(written, "xn--bcher-kva.xn--bcher-kva.xn--bcher-kva.a");
}

}  // namespace

int main() {
    an_a_label_that_decodes_to_nothing();
    the_first_error_survives_the_storage();
    the_nfc_copy_stops_the_driver_too();
    a_growing_label_goes_on();
    the_root_label();
    undecodable_labels_kept();
    the_ascii_form_as_given();
    return boost::report_errors();
}
