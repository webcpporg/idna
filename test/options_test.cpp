// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/idna/options.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string_view>

namespace idna = webcpp::idna;

namespace {

/** The defaults are the strict profile: every check of UTS 46 on, nontransitional, the DNS
    lengths verified, and IDNA2008 on top. */
void the_defaults_are_strict() {
    constexpr idna::options defaults;
    BOOST_TEST(defaults.check_hyphens);
    BOOST_TEST(defaults.check_bidi);
    BOOST_TEST(defaults.check_joiners);
    BOOST_TEST(defaults.use_std3_ascii_rules);
    BOOST_TEST(defaults.verify_dns_length);
    BOOST_TEST(!defaults.transitional_processing);
    BOOST_TEST(!defaults.ignore_invalid_punycode);
    BOOST_TEST(defaults.idna2008);
}

/** An aggregate: a caller names what changes and keeps the rest. */
void a_caller_names_what_changes() {
    constexpr idna::options uts46_alone{.idna2008 = false};
    BOOST_TEST(!uts46_alone.idna2008);
    BOOST_TEST(uts46_alone.check_bidi);
    BOOST_TEST(uts46_alone.verify_dns_length);
}

void the_limits_are_the_dns() {
    BOOST_TEST_EQ(idna::max_label_length, 63U);
    BOOST_TEST_EQ(idna::max_domain_length, 253U);
    BOOST_TEST_EQ(idna::unicode_version, std::string_view("17.0.0"));
}

}  // namespace

int main() {
    the_defaults_are_strict();
    a_caller_names_what_changes();
    the_limits_are_the_dns();
    return boost::report_errors();
}
