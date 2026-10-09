# Working on idna

webcpp's rules apply here: read the superproject's
[AGENTS.md](../../AGENTS.md)
(<https://github.com/webcpporg/webcpp/blob/main/AGENTS.md>) first. This file
holds only what is specific to idna.

## What idna is

A library of webcpp's own, not a port: `meta/libraries.json` has
`"port-of": null`, and idna has no oracle, no twins and no "Differences"
appendix. Its page, `doc/idna.adoc`, with the Doc Comments of its headers, is
what a change is checked against, and its conformance is Unicode's own test
files.

ada-url/idna is a reference for algorithms and table packing only: nothing of
it is copied, and no file of it enters this repository.

## The strict profile

`idna::options{}` is UTS 46 with every check on, nontransitional, the DNS
lengths verified, and `idna2008 = true`: IDNA2008's validity on top of
UTS 46's. UTS 46 alone accepts what IDNA2008 refuses (the code points it
marks NV8 and XV8, and every CONTEXTO violation, since it has no CONTEXTO
rules), and JSON Schema's `idn-hostname`, idna's first consumer through
`webcpp::formats`, asks for IDNA2008. The default never changes to a looser
one.

## Cautions

- **The errors are fixed.** `error`'s values 1 to 19 never change and are
  never reused; a new error takes the next value. The category's name,
  `webcpp.idna`, and its identity, `0x69646E6100000000`, never change either.
  `test/errors_test.cpp` pins them.
- **The public interface is a contract.** `webcpp::formats` codes against the
  signatures of `errors.hpp`, `options.hpp`, `punycode.hpp`, `validity.hpp`
  and `uts46.hpp`; a change to one is announced to it. Everything in
  `detail` may change freely.
- **No exceptions in a header.** The headers compile without exceptions,
  which the lint checks; a `noexcept` function reads a `result` it has
  checked with `unsafe_value()`, since Boost 1.92's `operator*` calls
  `value()`, which throws, and clang-tidy's `bugprone-exception-escape` then
  fails the function.
- **`config.hpp` comes first.** Every public header includes
  `<webcpp/idna/config.hpp>` first. It includes `<boost/config.hpp>` until
  `webcpp::config` exists; then that line becomes
  `#include <webcpp/config.hpp>`, and `build.jam`'s alias gains
  `<library>/webcpp/config//config`, in one commit.
- **Literals.** A narrow string spells a byte above 0x7F as a three-digit
  octal escape, `"b\303\274cher"`: a hex escape runs into the letters after
  it, and raw UTF-8 is read in the machine's code page by MSVC. A `U""`
  string uses `\u` and `\U`, and `\x` only for a value no universal character
  name may name. A default `options` is written `idna::options()`:
  clang-tidy's `readability-trailing-comma` misreads an empty `options{}`
  followed by a comma.

## The page

Each `@see "<title>"` of a Doc Comment names a top-level section of the page:
"Introduction", "Punycode", "Converting names", "Validity", "Errors",
"Allocation", "Testing". Renaming a section renames its `@see` lines in the
same commit; doc-check fails otherwise.

An A-label in the page's prose is written as literal monospace,
`` `+xn--bcher-kva.example+` ``: Asciidoctor replaces `--` between two word
characters with an em dash, inside backticks too, and no check of the build
sees it in the rendered page. `xn--` alone, followed by a backtick, is safe.
