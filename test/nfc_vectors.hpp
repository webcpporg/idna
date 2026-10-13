// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// The lines of NormalizationTest.txt as data, which tools/generate_tables.py writes into
// generated/nfc_test.ipp, so that the NFC test reads no file on any target.

#ifndef WEBCPP_TEST_IDNA_NFC_VECTORS_HPP
#define WEBCPP_TEST_IDNA_NFC_VECTORS_HPP

#include <array>
#include <string_view>

namespace webcpp::test {

// The generated rows' strings are sv literals, whose length the compiler knows.
using std::string_view_literals::operator""sv;

/** One line of NormalizationTest.txt: its first three columns. */
struct nfc_vector {
    /** Column 1, the source. */
    std::u32string_view source;

    /** Column 2, the NFC of every column 1 to 3. */
    std::u32string_view nfc;

    /** Column 3, the NFD. */
    std::u32string_view nfd;
};

#include "generated/nfc_test.ipp"

}  // namespace webcpp::test

#endif  // WEBCPP_TEST_IDNA_NFC_VECTORS_HPP
