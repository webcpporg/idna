# idna

idna is a header-only library of internationalized domain names for C++20,
one of the libraries of [webcpp](https://github.com/webcpporg/webcpp):
Punycode (RFC 3492), Unicode's UTS 46 processing, ToASCII and ToUnicode,
and the validity rules of IDNA2008 (RFC 5890 to 5893), over Unicode 17.0.0
tables generated into its headers.

Its default profile is UTS 46's mapping followed by IDNA2008's validity, the
strict profile JSON Schema's `idn-hostname` format asks for; `idna::options`
turns each check on or off. Every operation returns its error as a value,
the forms that write into a caller's buffer allocate nothing, and the forms
that return a string take a `std::pmr::memory_resource`.

`<webcpp/idna.hpp>` includes every public header; the names are in the
namespace `webcpp::idna`. idna needs Boost's headers and webcpp's
configuration, config, and nothing to build or link. It runs natively, on
wasm32-wasip2 and wasm32-wasip3, and with Emscripten.

## A minimal example

This is [example/idna_quick_start.cpp](example/idna_quick_start.cpp), which
converts two names to their ASCII form and an A-label back to Unicode:

<!-- include::example/idna_quick_start.cpp[tag=program] -->
```cpp
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
```

It prints:

<!-- include::example/idna_quick_start.expected[] -->
```
Bücher.Example -> xn--bcher-kva.example
a·b.example -> error: code point outside its allowed context
bücher.example
```

## Building and testing

idna is developed inside the webcpp superproject, as a Boost library is
developed inside Boost:

    git clone --recursive https://github.com/webcpporg/webcpp
    cd webcpp
    b2 libs/idna/test libs/idna/example

builds the tests and the examples natively, runs every example and compares
its output with the `.expected` file beside it.
`toolset=clang-wasip2 testing.launcher=wasmtime`,
`toolset=clang-wasip3 testing.launcher=wasmtime` and `toolset=emscripten`
do the same for WebAssembly. A b2 project uses idna through
`/webcpp/idna//idna`.

## Documentation

idna's page, with its API reference, is published at
<https://webcpporg.github.io/libs/idna/>; `b2 libs/idna/doc` builds
it into `doc/html/index.html`.

## Unicode data

idna's tables and the vectors of its tests are derived from Unicode's data
files, under the Unicode License v3, whose notice is the appendix "Unicode
data" of idna's page. idna's code is under the Boost Software License alone.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
