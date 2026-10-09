// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The six rules of RFC 5893 section 2 over one label, each satisfied and broken, and whether a
// label makes its domain a Bidi domain name.

#include <webcpp/idna/detail/bidi.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string_view>

namespace bidi = webcpp::idna::detail::bidi;

namespace {

/** The label is RTL in RFC 5893's sense, and satisfies the rules or not. */
void is(std::u32string_view label, bool rtl, bool satisfied) {
    const bidi::label_bidi found = bidi::check(label);
    BOOST_TEST_EQ(found.rtl, rtl);
    BOOST_TEST_EQ(found.satisfied, satisfied);
}

void rtl_labels() {
    is(U"\u05D0\u05D1", true, true);
    is(U"\u0627\u0644", true, true);          // AL
    is(U"\u05D0\u0300", true, true);          // rule 3: R, then NSM
    is(U"\u05D01", true, true);               // rule 3: ends with EN
    is(U"\u05D0\u0660", true, true);          // rule 3: ends with AN
    is(U"\u05D0a", true, false);              // rule 2: L in an RTL label
    is(U"\u05D0a\u05D0", true, false);        // rule 2 alone: L inside, R at the end
    is(U"\u05D0-", true, false);              // rule 3: ends with ES
    is(U"\u05D0!", true, false);              // rule 3: ends with ON
    is(U"\u05D00\u0660\u05D0", true, false);  // rule 4: EN and AN
}

/** Rule 2: each class it allows inside an RTL label, and one it does not (WS). */
void rtl_classes() {
    is(U"\u05D0\u0627", true, true);        // R, AL
    is(U"\u05D0-\u05D0", true, true);       // ES
    is(U"\u05D0,\u05D0", true, true);       // CS
    is(U"\u05D0$\u05D0", true, true);       // ET
    is(U"\u05D0!\u05D0", true, true);       // ON
    is(U"\u05D0\u00AD\u05D0", true, true);  // BN
    is(U"\u05D0 \u05D0", true, false);      // WS
}

void ltr_labels() {
    is(U"abc", false, true);
    is(U"a0", false, true);       // rule 6: ends with EN
    is(U"a\u0300", false, true);  // rule 6: L, then NSM
    is(U"a-", false, false);      // rule 6: ends with ES
    is(U"a\u05D0", true, false);  // rule 5: R in an LTR label
    is(U"a\u0627", true, false);  // rule 5: AL in an LTR label
    is(U"a\u0660", true, false);  // rule 5: AN in an LTR label
    // Rule 5 alone: the label ends with L, so rule 6 holds.
    is(U"a\u05D0b", true, false);
    is(U"a\u0627b", true, false);
    is(U"a\u0660b", true, false);
}

/** Rule 5: each class it allows inside an LTR label, and one it does not (WS). */
void ltr_classes() {
    is(U"a-b", false, true);       // ES
    is(U"a,b", false, true);       // CS
    is(U"a$b", false, true);       // ET
    is(U"a!b", false, true);       // ON
    is(U"a\u00ADb", false, true);  // BN
    is(U"a b", false, false);      // WS
}

/** Rule 1: the first code point is L, R or AL. */
void first_code_point() {
    is(U"0a", false, false);
    is(U"0\u05D0", true, false);
    is(U"\u0660", true, false);
    is(U"-a", false, false);
    is(U"!a", false, false);
    is(U"\u0300a", false, false);  // a leading combining mark
    is(U"\u0300\u05D0", true, false);
}

/** An empty label holds no code point: none of RTL and none of a rule's breach. */
void empty_label() {
    is(U"", false, true);
}

}  // namespace

int main() {
    rtl_labels();
    rtl_classes();
    ltr_labels();
    ltr_classes();
    first_code_point();
    empty_label();
    return boost::report_errors();
}
