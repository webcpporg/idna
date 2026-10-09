// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// How a conformance test holds its verdict on a line of IdnaTestV2.txt against
// test/known_failures.json, whose reason tools/generate_tables.py writes into the line's row: the
// list must never lie, so a line it does not name must pass, and a line it names must fail.

#ifndef WEBCPP_TEST_IDNA_KNOWN_FAILURES_HPP
#define WEBCPP_TEST_IDNA_KNOWN_FAILURES_HPP

#include <boost/core/lightweight_test.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace webcpp::test {

/**
 Whether the verdict on line `line` is the one test/known_failures.json foresees, a failure exactly
 when `known_failure` holds a reason; a verdict it does not foresee is reported as a failed check,
 naming the line.
*/
inline bool as_listed(std::uint32_t line, std::string_view known_failure, bool passed) {
    if (passed == known_failure.empty()) {
        return true;
    }
    std::string what = "IdnaTestV2.txt line " + std::to_string(line);
    if (passed) {
        what += " passes, and test/known_failures.json lists it: ";
        what += known_failure;
    } else {
        what += " fails, and test/known_failures.json does not list it";
    }
    BOOST_ERROR(what.c_str());
    return false;
}

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_IDNA_KNOWN_FAILURES_HPP
