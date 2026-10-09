// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The list of known failures holds both ways. Built as it is, the program checks that as_listed
// accepts the two verdicts the list foresees; built with WEBCPP_TEST_IDNA_UNEXPECTED_PASS or
// WEBCPP_TEST_IDNA_UNEXPECTED_FAILURE, it gives as_listed one it does not foresee, and test/Jamfile
// declares it with webcpp.run-fail: a conformance test fails as it does.

#include <boost/core/lightweight_test.hpp>

#include "known_failures.hpp"

using webcpp::test::as_listed;

int main() {
#ifdef WEBCPP_TEST_IDNA_UNEXPECTED_PASS
    // A listed line that passes: the list must lose it.
    as_listed(107, "a reason", true);
#elif defined(WEBCPP_TEST_IDNA_UNEXPECTED_FAILURE)
    // A line that fails and is not listed: a new failure.
    as_listed(107, "", false);
#else
    BOOST_TEST(as_listed(107, "", true));
    BOOST_TEST(as_listed(107, "a reason", false));
#endif
    return boost::report_errors();
}
