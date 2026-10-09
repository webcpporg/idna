// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/idna/punycode.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace idna = webcpp::idna;

int main() {
    // tag::encode[]
    // RFC 3492 section 7.1's sample (L).
    const std::u32string_view sample = U"3\u5E74B\u7D44\u91D1\u516B\u5148\u751F";
    std::array<char, 64> encoded{};
    const idna::result<std::size_t> written = idna::punycode::encode(encoded, sample);
    std::cout << std::string_view(encoded.data(), written.value()) << '\n';
    // end::encode[]

    // tag::decode[]
    std::array<char32_t, 64> decoded{};
    const idna::result<std::size_t> read =
        idna::punycode::decode(decoded, "3B-ww4c5e180e575a65lsy2b");
    std::cout << read.value() << " code points, the same: "
              << (std::u32string_view(decoded.data(), read.value()) == sample ? "yes" : "no")
              << '\n';
    // end::decode[]

    // tag::errors[]
    for (const std::string_view bad : {"-9uc", "bcher-k", "999999999999999999999"}) {
        std::cout << bad << " -> " << idna::punycode::decode(decoded, bad).error().message()
                  << '\n';
    }
    // end::errors[]
}
