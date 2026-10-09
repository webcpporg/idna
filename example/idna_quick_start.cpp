// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// tag::program[]
#include <webcpp/idna.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace idna = webcpp::idna;

int main() {
    std::array<char, idna::max_domain_length> ascii{};
    for (const std::string_view name : {"B\303\274cher.Example", "a\302\267b.example"}) {
        const idna::result<std::size_t> converted = idna::to_ascii(ascii, name);
        if (converted.has_error()) {
            std::cout << name << " -> error: " << converted.error().message() << '\n';
            continue;
        }
        std::cout << name << " -> " << std::string_view(ascii.data(), converted.value()) << '\n';
    }

    std::array<char, 1024> unicode{};
    const idna::unicode_result shown = idna::to_unicode(unicode, "xn--bcher-kva.example");
    std::cout << std::string_view(unicode.data(), shown.size)
              << (shown.error ? " (with an error)" : "") << '\n';
}

// end::program[]
