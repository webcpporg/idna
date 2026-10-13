// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The lines of IdnaTestV2.txt as data, which tools/generate_tables.py writes into
// generated/idna_test_v2.ipp, so that the conformance test reads no file on any target.

#ifndef WEBCPP_TEST_IDNA_IDNA_VECTORS_HPP
#define WEBCPP_TEST_IDNA_IDNA_VECTORS_HPP

#include <array>
#include <cstdint>
#include <string_view>

namespace webcpp::test {

// The generated rows' strings are sv literals, whose length the compiler knows.
using std::string_view_literals::operator""sv;

/** One line of IdnaTestV2.txt, its blank fields resolved as the file's header says. */
struct idna_vector {
    /** The line's number in the pinned file. */
    std::uint32_t line;

    /** The source, in UTF-8; ill-formed where the file escapes a lone surrogate. */
    std::string_view source;

    /** toUnicode's result. */
    std::string_view to_unicode;

    /** Whether toUnicode records an error. */
    bool to_unicode_error;

    /** toAsciiN's result, nontransitional. */
    std::string_view to_ascii_n;

    /** Whether toAsciiN records an error. */
    bool to_ascii_n_error;

    /** toAsciiT's result, transitional. */
    std::string_view to_ascii_t;

    /** Whether toAsciiT records an error. */
    bool to_ascii_t_error;

    /** Whether the toUnicode field holds no NV8, XV8 or CONTEXTO code point, so that the strict
        profile must give the same results. */
    bool strict;

    /** Why the line is a known failure, from test/known_failures.json; empty for every other. */
    std::string_view known_failure;
};

#include "generated/idna_test_v2.ipp"

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_IDNA_IDNA_VECTORS_HPP
