// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/idna/validity.hpp>

#include <iostream>
#include <string_view>

namespace idna = webcpp::idna;

int main() {
    // tag::labels[]
    // "\302\267" is U+00B7 MIDDLE DOT in UTF-8, which RFC 5892 allows only between two 'l'.
    for (const std::string_view label :
         {"example", "xn--bcher-kva", "l\302\267l", "a\302\267l", "ab--cd", "Example"}) {
        const idna::result<void> checked = idna::check_label(label);
        std::cout << label << ": " << (checked.has_value() ? "valid" : checked.error().message())
                  << '\n';
    }
    // end::labels[]

    // tag::domains[]
    // "\327\220" is U+05D0 HEBREW LETTER ALEF in UTF-8, a right-to-left letter.
    for (const std::string_view domain : {"a.\327\220", "0a.\327\220", "0a.b"}) {
        std::cout << domain << ": " << (idna::is_valid_domain(domain) ? "valid" : "invalid")
                  << '\n';
    }
    // end::domains[]

    // tag::uts46_alone[]
    const idna::options uts46_alone{.idna2008 = false};
    std::cout << "a\302\267l under UTS 46 alone: "
              << (idna::is_valid_label("a\302\267l", uts46_alone) ? "valid" : "invalid") << '\n';
    // end::uts46_alone[]
}
