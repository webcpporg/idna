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
namespace `webcpp::idna`. idna needs Boost's headers and nothing to build or
link. It runs natively, on wasm32-wasip2 and wasm32-wasip3, and with
Emscripten.

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
<https://webcpporg.github.io/webcpp/libs/idna/>; `b2 libs/idna/doc` builds
it into `doc/html/index.html`.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
