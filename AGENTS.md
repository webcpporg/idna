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
  `<webcpp/idna/config.hpp>` first, which includes `<webcpp/config.hpp>` first
  and defines `WEBCPP_IDNA_NO_EXCEPTIONS` when webcpp has no exceptions
  (`WEBCPP_HAS_EXCEPTIONS` is not defined: the compiler has none, or the
  program defined `WEBCPP_NO_EXCEPTIONS`) or when a program defines it for
  idna alone; the tests `config`, `config_developer_defined` and
  `config_all_off` check the three ways.
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
"Allocation", "Limits", "Testing". Renaming a section renames its `@see`
lines in the same commit; doc-check fails otherwise.

An A-label in the page's prose is written as literal monospace,
`` `+xn--bcher-kva.example+` ``: Asciidoctor replaces `--` between two word
characters with an em dash, inside backticks too, and no check of the build
sees it in the rendered page. `xn--` alone, followed by a backtick, is safe.

## Unicode and the tables

The version is 17.0.0, UTS 46 revision 35: `idna::unicode_version`, and the
page's attributes `:unicode-version:` and `:uts46-revision:`. Unicode 18.0.0
(UTS 46 revision 36) is out; the move is not made. A move is a change of its
own, and the page's section "Moving to another version of Unicode"
(`doc/unicode.adoc`) lists every file it changes.

`include/webcpp/idna/detail/tables.hpp` is generated: never edit it by hand.
`tools/generate_tables.py` writes it from the Unicode files that
`tools/unicode.json` pins by URL and SHA-256. It is run from the
superproject's root by `update-generated`, which writes every generated file
in place, or none when the generator fails (the check of the generated
files, below):

```bash
b2 libs/idna/test//update-generated
```

Outside a build, `python3 libs/idna/tools/generate_tables.py` writes the
same files in place itself. The generator downloads a missing file into
`bin/unicode/<version>/` (`--cache <dir>` names another place), stops on a
digest that differs from the pin, and checks what the library assumes of the
data before it writes anything (its docstring lists the checks). A property
value it does not know, such as a script that a new Unicode version adds,
stops it by name: the value joins its list in the generator once it is known
not to matter to IDNA. Two runs write the same bytes. The packing of each
array is stated in the Doc Comment above it, in the generated file, and
`detail/properties.hpp` holds the only code that reads it.

- **IDNA2008's property is derived, not shipped.** A code point is PVALID,
  CONTEXTJ or CONTEXTO exactly when its UTS 46 status is valid or deviation
  without an NV8 or XV8 mark. The generator stops if IANA's
  `idna-tables-12.0.0` disagrees for any code point IANA assigns, U+002E
  aside, or if the CONTEXTJ and CONTEXTO sets differ from RFC 5892
  appendix A, whose rules `detail/context.hpp` hard-codes.
- **The label storage rests on a measurement.** `detail::max_label_code_points`
  is 256 because no full canonical decomposition is longer than 4 code
  points (U+1FAF); the generator stops if a new version breaks that.
- **The data notice.** The generated files derive from Unicode's data, under
  the Unicode License v3: each names the page's appendix "Unicode data" in
  one comment line, the appendix holds the notice whole, and idna's code is
  BSL-1.0 alone.
- **The vectors are generated too.** `test/generated/idna_test_v2.ipp` and
  `test/generated/nfc_test.ipp` hold IdnaTestV2.txt and the first three
  columns of NormalizationTest.txt as rows, so no test reads a file on any
  target. `test/known_failures.json` lists the IdnaTestV2 lines expected to
  fail, by number and source (as the file writes it, escapes included), each
  with its reason; the generator reads it and writes each reason into its
  line's row, so a change to the list is followed by a run of the generator.
  A conformance test holds each verdict against the row through
  `test/known_failures.hpp`'s `as_listed`, which fails on a listed line that
  passes as on an unlisted line that fails. A listed line must fail under
  UTS 46 alone, and the strict pass leaves it out. The list is empty, and may
  hold only a failure that comes from a difference between two versions of
  Unicode.
- **The check of the generated files** is the own lane `tables`:
  `b2 -a libs/idna/test//tables` runs the generator into the build directory
  and compares its three outputs with the committed files, and
  `b2 libs/idna/test//update-generated` writes them in place: in a build, it
  is the one writer of the generated files. The ordinary lanes never run it;
  it needs the network until the cache is filled.
