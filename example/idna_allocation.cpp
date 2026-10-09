// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/idna/uts46.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <string>
#include <string_view>

namespace idna = webcpp::idna;

int main() {
    // tag::span[]
    // A span form writes into the caller's buffer and allocates nothing.
    std::array<char, idna::max_domain_length> buffer{};
    const idna::result<std::size_t> written = idna::to_ascii(buffer, "b\303\274cher.example");
    std::cout << std::string_view(buffer.data(), written.value()) << '\n';
    // end::span[]

    // tag::pmr[]
    // A pmr form takes its memory from the resource it is given, here a buffer on the stack.
    std::array<std::byte, 16384> storage{};
    std::pmr::monotonic_buffer_resource resource(storage.data(), storage.size(),
                                                 std::pmr::null_memory_resource());
    const idna::result<std::pmr::string> owned =
        idna::to_ascii("b\303\274cher.example", idna::options(), resource);
    std::cout << owned.value() << '\n';
    // end::pmr[]

    // tag::long_label[]
    // Without the DNS lengths, a label of 300 code points is beyond the inline storage: the span
    // form refuses it, and the pmr form holds it in its resource.
    const std::string long_label(300, 'a');
    const idna::options no_lengths{.verify_dns_length = false};
    std::array<char, 512> wide{};
    std::cout << idna::to_ascii(wide, long_label, no_lengths).error().message() << '\n';
    std::cout << idna::to_ascii(long_label, no_lengths, resource).value().size() << " octets\n";
    // end::long_label[]
}
