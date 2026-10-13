#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)

"""Writes idna's Unicode tables and its conformance vectors from the pinned Unicode files.

Usage: generate_tables.py [--cache DIR] [TABLES IDNA_VECTORS NFC_VECTORS]

The inputs are the files tools/unicode.json pins, each by its URL and its SHA-256. A file missing
from the cache directory (default: bin/unicode/<version>/ of the superproject) is downloaded into
it; every file is hashed, and a digest that differs from the pinned one stops the generator, naming
the file (or the URL it was downloaded from, which then leaves nothing in the cache) and both
digests, before anything is written.

The three outputs are written at the paths given, in this order, or by default at their places in
the library: include/webcpp/idna/detail/tables.hpp, the tables as constexpr data;
test/generated/idna_test_v2.ipp, IdnaTestV2.txt as rows; test/generated/nfc_test.ipp, the first
three columns of NormalizationTest.txt as rows. webcpp.generated runs it with paths in its build
directory, and compares the outputs with the committed files, or copies them over the committed
files for update-generated. Two runs write the same bytes: no date, sorted data, "\\n" line
endings. Nothing is written unless every check passed.

Before writing, the generator checks what the library assumes of the data, and stops naming the
assumption that fails: every status of IdnaMappingTable.txt is one of the five it knows; a code
point is PVALID, CONTEXTJ or CONTEXTO in IANA's IDNA2008 tables exactly when its UTS 46 status is
valid or deviation without an NV8 or XV8 mark, U+002E aside; the CONTEXTJ and CONTEXTO code points
are those of RFC 5892 appendix A; no full canonical decomposition is longer than 4 code points; no
Hangul syllable has a decomposition in UnicodeData.txt (the library composes and decomposes them
algorithmically); and every value a file holds is known, either one the tables keep or one listed
as reading as other. A new Unicode version that adds a value (a script, say) stops the generator
naming it, and the value joins its list once it is known not to matter to IDNA.

It checks the test files the same way: every line of IdnaTestV2.txt has its seven fields, each
status field is blank or a list of the status codes the file's header describes, and every escape
is a \\uXXXX that stands for one code point (a pair of escaped surrogates, which could mean one
code point or two, stops it); every line of NormalizationTest.txt has its five columns of code
points, none a surrogate. And test/known_failures.json is for the pinned version of Unicode, and
each of its entries names a line of IdnaTestV2.txt by its number and its source, as the file
writes it, once, with a reason.

Exit 0 when every output was written, 1 naming a fault, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import re
import sys
import textwrap
import urllib.request
from collections.abc import Iterator, Sequence
from pathlib import Path

LIBRARY = Path(__file__).resolve().parent.parent
SUPERPROJECT = LIBRARY.parent.parent
PINNED = LIBRARY / 'tools' / 'unicode.json'
KNOWN_FAILURES = LIBRARY / 'test' / 'known_failures.json'

TABLES = 'include/webcpp/idna/detail/tables.hpp'
IDNA_VECTORS = 'test/generated/idna_test_v2.ipp'
NFC_VECTORS = 'test/generated/nfc_test.ipp'
OUTPUTS = (TABLES, IDNA_VECTORS, NFC_VECTORS)

LAST_CODE_POINT = 0x10FFFF
STATUSES = ('valid', 'mapped', 'deviation', 'ignored', 'disallowed')
MARKERS = ('NV8', 'XV8')
IANA_PROPERTIES = ('PVALID', 'CONTEXTJ', 'CONTEXTO', 'DISALLOWED', 'UNASSIGNED')

# Each property's values the tables keep, in the order of their enumeration (0 is other), and the
# values of Unicode 17.0.0 that read as other. A value of neither kind stops the generator: a new
# Unicode version that adds or renames one is looked at, never absorbed as other.
BIDI_CLASSES = ('other', 'L', 'R', 'AL', 'AN', 'EN', 'ES', 'CS', 'ET', 'ON', 'BN', 'NSM')
BIDI_OTHERS = frozenset({'B', 'S', 'WS', 'LRE', 'LRO', 'RLE', 'RLO', 'PDF', 'LRI', 'RLI', 'FSI',
                         'PDI'})
BIDI_LONG_NAMES = {
    'Left_To_Right': 'L', 'Right_To_Left': 'R', 'Arabic_Letter': 'AL', 'Arabic_Number': 'AN',
    'European_Number': 'EN', 'European_Separator': 'ES', 'Common_Separator': 'CS',
    'European_Terminator': 'ET', 'Other_Neutral': 'ON', 'Boundary_Neutral': 'BN',
    'Nonspacing_Mark': 'NSM',
}
JOINING_TYPES = ('other', 'L', 'D', 'R', 'T')
JOINING_OTHERS = frozenset({'C', 'U'})
JOINING_LONG_NAMES = {
    'Non_Joining': 'U', 'Join_Causing': 'C', 'Dual_Joining': 'D', 'Left_Joining': 'L',
    'Right_Joining': 'R', 'Transparent': 'T',
}
SCRIPTS = ('other', 'Greek', 'Hebrew', 'Hiragana', 'Katakana', 'Han')
SCRIPT_OTHERS = frozenset('''
    Adlam Ahom Anatolian_Hieroglyphs Arabic Armenian Avestan Balinese Bamum Bassa_Vah Batak Bengali
    Beria_Erfe Bhaiksuki Bopomofo Brahmi Braille Buginese Buhid Canadian_Aboriginal Carian
    Caucasian_Albanian Chakma Cham Cherokee Chorasmian Common Coptic Cuneiform Cypriot Cypro_Minoan
    Cyrillic Deseret Devanagari Dives_Akuru Dogra Duployan Egyptian_Hieroglyphs Elbasan Elymaic
    Ethiopic Garay Georgian Glagolitic Gothic Grantha Gujarati Gunjala_Gondi Gurmukhi Gurung_Khema
    Hangul Hanifi_Rohingya Hanunoo Hatran Imperial_Aramaic Inherited Inscriptional_Pahlavi
    Inscriptional_Parthian Javanese Kaithi Kannada Kawi Kayah_Li Kharoshthi Khitan_Small_Script
    Khmer Khojki Khudawadi Kirat_Rai Lao Latin Lepcha Limbu Linear_A Linear_B Lisu Lycian Lydian
    Mahajani Makasar Malayalam Mandaic Manichaean Marchen Masaram_Gondi Medefaidrin Meetei_Mayek
    Mende_Kikakui Meroitic_Cursive Meroitic_Hieroglyphs Miao Modi Mongolian Mro Multani Myanmar
    Nabataean Nag_Mundari Nandinagari New_Tai_Lue Newa Nko Nushu Nyiakeng_Puachue_Hmong Ogham
    Ol_Chiki Ol_Onal Old_Hungarian Old_Italic Old_North_Arabian Old_Permic Old_Persian Old_Sogdian
    Old_South_Arabian Old_Turkic Old_Uyghur Oriya Osage Osmanya Pahawh_Hmong Palmyrene Pau_Cin_Hau
    Phags_Pa Phoenician Psalter_Pahlavi Rejang Runic Samaritan Saurashtra Sharada Shavian Siddham
    Sidetic SignWriting Sinhala Sogdian Sora_Sompeng Soyombo Sundanese Sunuwar Syloti_Nagri Syriac
    Tagalog Tagbanwa Tai_Le Tai_Tham Tai_Viet Tai_Yo Takri Tamil Tangsa Tangut Telugu Thaana Thai
    Tibetan Tifinagh Tirhuta Todhri Tolong_Siki Toto Tulu_Tigalari Ugaritic Unknown Vai Vithkuqi
    Wancho Warang_Citi Yezidi Yi Zanabazar_Square
'''.split())
QUICK_CHECKS = ('Y', 'N', 'M')
MARK_CATEGORIES = ('Mn', 'Mc', 'Me')
OTHER_CATEGORIES = frozenset('''
    Cc Cf Cn Co Cs Ll Lm Lo Lt Lu Nd Nl No Pc Pd Pe Pf Pi Po Ps Sc Sk Sm So Zl Zp Zs
'''.split())
CONTEXTJ = frozenset({0x200C, 0x200D})
CONTEXTO = frozenset({0x00B7, 0x0375, 0x05F3, 0x05F4, 0x30FB,
                      *range(0x0660, 0x066A), *range(0x06F0, 0x06FA)})
FULL_STOP = 0x002E
HANGUL_FIRST, HANGUL_LAST = 0xAC00, 0xD7A3
MAX_DECOMPOSITION = 4
MAPPING_LENGTH_BITS, MAPPING_OFFSET_BITS = 5, 23
SURROGATES = range(0xD800, 0xE000)

# The test files' formats, as their headers state them: IdnaTestV2.txt's seven fields, its status
# codes (Pn, Vn, U1, An, Bn, Cn and Xn, a step and a substep), its one escape, and
# NormalizationTest.txt's five columns, each line ending with a semicolon.
IDNA_TEST_FIELDS = 7
STATUS_CODE = re.compile(r'[PVABCX][0-9]+(?:_[0-9]+)?|U1')
ESCAPE = re.compile(r'\\u([0-9A-Fa-f]{4})')
NORMALIZATION_TEST_COLUMNS = 5
BIDI_CONTROLS = frozenset({*range(0x202A, 0x202F), *range(0x2066, 0x206A)})
BIDI_CHECK = 'misc-misleading-bidirectional'
POSITIONAL_ROWS = ('modernize-use-designated-initializers',
                   'The rows are positional, as designated initializers would double the file.')

SV_LITERALS = ('Each string is an sv literal, whose length the compiler knows: a view made from a '
               'pointer would count its characters in the constant evaluation of the array, '
               'which every compiler bounds, and Clang on libstdc++ runs out of steps there.')

NOTICE = """\
// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
// Generated by tools/generate_tables.py from the Unicode {version} data files: do not edit.
// Unicode License v3: its notice is in the appendix "Unicode data" of idna's page.
"""


class Fault(Exception):
    """A reason the generator stops, said to the user."""


# Inputs


def pinned_files(pinned: Path) -> tuple[str, dict[str, dict[str, str]]]:
    """The Unicode version and each pinned file by its path in the cache."""
    data = json.loads(pinned.read_text(encoding='utf-8'))
    return data['version'], {entry['path']: entry for entry in data['files']}


def write(path: Path, data: bytes) -> None:
    """Writes the data at the path through a sibling file, so that no reader sees half of it."""
    path.parent.mkdir(parents=True, exist_ok=True)
    partial = path.with_name(path.name + '.partial')
    partial.write_bytes(data)
    partial.replace(path)


def checked(data: bytes, source: str, pinned: str) -> bytes:
    """The data, unless their SHA-256 differs from the pinned one."""
    digest = hashlib.sha256(data).hexdigest()
    if digest != pinned:
        raise Fault(f'{source}: SHA-256 {digest}, but tools/unicode.json pins {pinned}')
    return data


def fetch(entry: dict[str, str], cache: Path) -> str:
    """The text of one pinned file, downloaded into the cache when missing, its digest checked.

    A download is checked before it is kept, so that a moved URL, which may answer with another
    page, leaves nothing in the cache."""
    path = cache / entry['path']
    if path.is_file():
        return checked(path.read_bytes(), str(path), entry['sha256']).decode('utf-8')
    with urllib.request.urlopen(entry['url'], timeout=60) as response:
        body = checked(response.read(), entry['url'], entry['sha256'])
    write(path, body)
    return body.decode('utf-8')


def records(text: str) -> Iterator[tuple[int, int, list[str]]]:
    """Each data line of a UCD file as its first and last code points and its other fields."""
    for line in text.splitlines():
        body = line.split('#', 1)[0].strip()
        if not body:
            continue
        fields = [field.strip() for field in body.split(';')]
        first, _, last = fields[0].partition('..')
        yield int(first, 16), int(last or first, 16), fields[1:]


def missing(text: str) -> Iterator[tuple[int, int, list[str]]]:
    """Each `@missing` line of a UCD file as its range and its other fields (a default value, or a
    property and its default value)."""
    for line in text.splitlines():
        if line.startswith('# @missing:'):
            span, *fields = (part.strip() for part in line[len('# @missing:'):].split(';'))
            first, _, last = span.partition('..')
            yield int(first, 16), int(last or first, 16), fields


def property_of(text: str, source: str, names: Sequence[str], others: frozenset[str],
                aliases: dict[str, str]) -> list[int]:
    """A property's value for every code point, as an index into `names`, 0 for one of `others`;
    a long name of `@missing` is read through `aliases`, and any other value stops the generator."""
    values = [0] * (LAST_CODE_POINT + 1)

    def index(name: str, first: int) -> int:
        name = aliases.get(name, name)
        if name in names:
            return names.index(name)
        if name in others:
            return 0
        raise Fault(f'{source}: unknown value {name!r} at {first:04X}')

    for first, last, fields in missing(text):
        values[first:last + 1] = [index(fields[0], first)] * (last - first + 1)
    for first, last, fields in records(text):
        values[first:last + 1] = [index(fields[0], first)] * (last - first + 1)
    return values


# The UTS 46 mapping table


class Mapping:
    """The UTS 46 status, IDNA2008 mark and mapping of every code point."""

    def __init__(self, text: str) -> None:
        self.status = [STATUSES.index('disallowed')] * (LAST_CODE_POINT + 1)
        self.marked = [False] * (LAST_CODE_POINT + 1)
        self.target: list[tuple[int, ...]] = [()] * (LAST_CODE_POINT + 1)
        for first, last, fields in records(text):
            if fields[0] not in STATUSES:
                raise Fault(f'IdnaMappingTable.txt: unknown status {fields[0]!r} at {first:04X}')
            target = tuple(int(cp, 16) for cp in fields[1].split()) if len(fields) > 1 else ()
            marker = fields[2] if len(fields) > 2 else ''
            if marker not in ('', *MARKERS):
                raise Fault(f'IdnaMappingTable.txt: unknown IDNA2008 status {marker!r} at '
                            f'{first:04X}')
            marked = marker in MARKERS
            for cp in range(first, last + 1):
                self.status[cp] = STATUSES.index(fields[0])
                self.marked[cp] = marked
                self.target[cp] = target

    def idna2008_valid(self, cp: int) -> bool:
        """Whether IDNA2008 makes `cp` PVALID, CONTEXTJ or CONTEXTO, by UTS 46's table."""
        return STATUSES[self.status[cp]] in ('valid', 'deviation') and not self.marked[cp]


def check_against_iana(mapping: Mapping, text: str) -> None:
    """Stops unless the derivation from UTS 46's table agrees with IANA's IDNA2008 tables, and the
    context sets with RFC 5892 appendix A."""
    disagree: list[int] = []
    contextj: set[int] = set()
    contexto: set[int] = set()
    reader = csv.DictReader(io.StringIO(text))
    for column in ('Codepoint', 'Property'):
        if column not in (reader.fieldnames or []):
            raise Fault(f'idna-tables-properties.csv: no column {column!r}')
    for row in reader:
        first, _, last = row['Codepoint'].partition('-')
        prop = row['Property']
        if prop not in IANA_PROPERTIES:
            raise Fault(f'idna-tables-properties.csv: unknown property {prop!r} at {first}')
        for cp in range(int(first, 16), int(last or first, 16) + 1):
            if prop == 'UNASSIGNED':
                continue
            if prop == 'CONTEXTJ':
                contextj.add(cp)
            if prop == 'CONTEXTO':
                contexto.add(cp)
            if mapping.idna2008_valid(cp) != (prop != 'DISALLOWED') and cp != FULL_STOP:
                disagree.append(cp)
    if disagree:
        shown = ', '.join(f'U+{cp:04X}' for cp in disagree[:10])
        raise Fault(f'IANA idna-tables-12.0.0 and the derivation from UTS 46 disagree on {shown}')
    if contextj != CONTEXTJ or contexto != CONTEXTO:
        raise Fault('IANA idna-tables-12.0.0 lists other CONTEXTJ or CONTEXTO code points than '
                    'RFC 5892 appendix A')


# Normalization


def unicode_data(text: str) -> Iterator[tuple[int, int, list[str]]]:
    """Each code point or First/Last range of UnicodeData.txt with its fields."""
    pending: tuple[int, list[str]] | None = None
    for line in text.splitlines():
        fields = line.split(';')
        cp = int(fields[0], 16)
        if fields[1].endswith(', First>'):
            pending = (cp, fields)
            continue
        if pending is not None:
            yield pending[0], cp, pending[1]
            pending = None
            continue
        yield cp, cp, fields


def decompositions(text: str) -> dict[int, tuple[int, ...]]:
    """Every canonical decomposition of UnicodeData.txt, one level deep, Hangul aside."""
    found: dict[int, tuple[int, ...]] = {}
    for first, _, fields in unicode_data(text):
        if fields[5] and not fields[5].startswith('<'):
            found[first] = tuple(int(cp, 16) for cp in fields[5].split())
    return found


def full_decomposition(cp: int, immediate: dict[int, tuple[int, ...]]) -> tuple[int, ...]:
    """The full canonical decomposition of a code point, by applying the immediate ones until none
    applies."""
    pending = [cp]
    result: list[int] = []
    while pending:
        head = pending.pop(0)
        if head in immediate:
            pending[0:0] = immediate[head]
        else:
            result.append(head)
    return tuple(result)


def marks(text: str) -> list[int]:
    """1 for every code point of General_Category Mark, 0 for the others."""
    values = [0] * (LAST_CODE_POINT + 1)
    for first, last, fields in unicode_data(text):
        if fields[2] not in MARK_CATEGORIES and fields[2] not in OTHER_CATEGORIES:
            raise Fault(f'UnicodeData.txt: unknown General_Category {fields[2]!r} at {first:04X}')
        if fields[2] in MARK_CATEGORIES:
            values[first:last + 1] = [1] * (last - first + 1)
    return values


def composition_exclusions(text: str) -> set[int]:
    """The code points of Full_Composition_Exclusion."""
    excluded: set[int] = set()
    for first, last, fields in records(text):
        if fields[0] == 'Full_Composition_Exclusion':
            excluded.update(range(first, last + 1))
    return excluded


def quick_checks(text: str) -> list[int]:
    """NFC_Quick_Check of every code point, as an index into QUICK_CHECKS (Yes when absent)."""
    for first, _, fields in missing(text):
        if fields[0] == 'NFC_QC' and fields[1:] != ['Yes']:
            raise Fault(f'DerivedNormalizationProps.txt: NFC_QC defaults to {fields[1:]} at '
                        f'{first:04X}, not Yes')
    values = [0] * (LAST_CODE_POINT + 1)
    for first, last, fields in records(text):
        if fields[0] != 'NFC_QC':
            continue
        if fields[1] not in QUICK_CHECKS:
            raise Fault(f'DerivedNormalizationProps.txt: unknown NFC_QC value {fields[1]!r} at '
                        f'{first:04X}')
        values[first:last + 1] = [QUICK_CHECKS.index(fields[1])] * (last - first + 1)
    return values


def combining_classes(text: str) -> list[int]:
    """Canonical_Combining_Class of every code point (0, Not_Reordered, when absent)."""
    for first, _, fields in missing(text):
        if fields != ['Not_Reordered']:
            raise Fault(f'DerivedCombiningClass.txt: the default is {fields} at {first:04X}, '
                        'not Not_Reordered')
    values = [0] * (LAST_CODE_POINT + 1)
    for first, last, fields in records(text):
        if not fields[0].isdigit() or int(fields[0]) > 0xFF:
            raise Fault(f'DerivedCombiningClass.txt: {fields[0]!r} at {first:04X} is not a class '
                        'of 0 to 255')
        values[first:last + 1] = [int(fields[0])] * (last - first + 1)
    return values


# C++ output


def runs(values: Sequence[int]) -> list[tuple[int, int]]:
    """The values as runs: each first code point whose value differs from the one before it."""
    found: list[tuple[int, int]] = []
    for cp, value in enumerate(values):
        if not found or found[-1][1] != value:
            found.append((cp, value))
    return found


def rows(items: Sequence[str], indent: str = '    ', width: int = 100) -> str:
    """The items, comma-separated, as many on a line as fit the width."""
    lines: list[str] = []
    line = indent
    for item in items:
        piece = item + ','
        if len(line) + len(piece) + 1 > width and line.strip():
            lines.append(line.rstrip())
            line = indent
        line += piece + ' '
    if line.strip():
        lines.append(line.rstrip())
    return '\n'.join(lines)


def comment(brief: str, width: int = 100) -> str:
    """A Doc Comment of the brief: on one line when it fits the width, else wrapped inside."""
    if len(f'/** {brief} */') <= width:
        return f'/** {brief} */\n'
    wrapped = textwrap.fill(brief, width, initial_indent=' ', subsequent_indent=' ')
    return f'/**\n{wrapped}\n*/\n'


def array(brief: str, kind: str, name: str, items: Sequence[str]) -> str:
    """A documented `inline constexpr std::array` of the items."""
    return (comment(brief) +
            f'inline constexpr std::array<{kind}, {len(items)}> {name}{{\n'
            f'{rows(items)}\n'
            '};\n')


def enumeration(brief: str, name: str, enumerators: Sequence[tuple[str, str]]) -> str:
    """A documented enum class over std::uint8_t, each enumerator documented and valued in
    order."""
    body = '\n'.join(f'    /** {doc} */\n    {label} = {value},'
                     for value, (label, doc) in enumerate(enumerators))
    return f'/** {brief} */\nenum class {name} : std::uint8_t {{\n{body}\n}};\n'


def packed(values: Sequence[int]) -> list[str]:
    """The runs of a small property, each packed as its first code point shifted left by 8 and
    its value."""
    return [f'0x{first << 8 | value:X}' for first, value in runs(values)]


def uts46_rows(mapping: Mapping) -> tuple[list[str], list[str]]:
    """The UTS 46 runs, as packed values, and the code points of every mapping, in order."""
    offsets: dict[tuple[int, ...], int] = {}
    data: list[int] = []
    values: list[int] = []
    for cp in range(LAST_CODE_POINT + 1):
        target = mapping.target[cp]
        if target not in offsets:
            offsets[target] = len(data)
            data.extend(target)
        if len(target) >= 1 << MAPPING_LENGTH_BITS or offsets[target] >= 1 << MAPPING_OFFSET_BITS:
            raise Fault(f'the mapping of U+{cp:04X} does not fit the packed UTS 46 value')
        values.append(mapping.status[cp] | int(mapping.marked[cp]) << 3 | len(target) << 4
                      | offsets[target] << 9)
    return ([f'0x{first << 32 | value:X}' for first, value in runs(values)],
            [f'0x{cp:X}' for cp in data])


def tables_header(version: str, inputs: dict[str, str], mapping: Mapping) -> str:
    """The text of include/webcpp/idna/detail/tables.hpp."""
    check_against_iana(mapping, inputs['iana/idna-tables-properties.csv'])
    unicode = inputs['ucd/UnicodeData.txt']
    normalization = inputs['ucd/DerivedNormalizationProps.txt']
    immediate = decompositions(unicode)
    full = {cp: full_decomposition(cp, immediate) for cp in sorted(immediate)}
    longest = max(len(target) for target in full.values())
    if longest > MAX_DECOMPOSITION:
        raise Fault(f'a full canonical decomposition holds {longest} code points, more than '
                    f'the {MAX_DECOMPOSITION} the label storage assumes')
    hangul = [cp for cp in full if HANGUL_FIRST <= cp <= HANGUL_LAST]
    if hangul:
        raise Fault(f'U+{hangul[0]:04X}, a Hangul syllable, has a decomposition in '
                    'UnicodeData.txt, which the library computes algorithmically')
    excluded = composition_exclusions(normalization)
    composites = sorted((pair[0] << 21 | pair[1], cp) for cp, pair in immediate.items()
                        if len(pair) == 2 and cp not in excluded)
    decomposition_data: list[int] = []
    slices: list[str] = []
    for target in full.values():
        slices.append(f'0x{len(decomposition_data) << 3 | len(target):X}')
        decomposition_data.extend(target)
    uts46, mappings = uts46_rows(mapping)
    bidi = property_of(inputs['ucd/extracted/DerivedBidiClass.txt'], 'DerivedBidiClass.txt',
                       BIDI_CLASSES, BIDI_OTHERS, BIDI_LONG_NAMES)
    joining = property_of(inputs['ucd/extracted/DerivedJoiningType.txt'],
                          'DerivedJoiningType.txt', JOINING_TYPES, JOINING_OTHERS,
                          JOINING_LONG_NAMES)
    scripts = property_of(inputs['ucd/Scripts.txt'], 'Scripts.txt', SCRIPTS, SCRIPT_OTHERS, {})
    parts = [
        NOTICE.format(version=version),
        '/**\n The Unicode ' + version + ' tables idna reads: generated, and read through'
        ' detail/properties.hpp alone.\n*/\n',
        '#ifndef WEBCPP_IDNA_DETAIL_TABLES_HPP\n#define WEBCPP_IDNA_DETAIL_TABLES_HPP\n\n'
        '#include <webcpp/idna/config.hpp>\n\n#include <array>\n#include <cstdint>\n'
        '#include <string_view>\n\nnamespace webcpp::idna::detail::tables {\n',
        f'/** The version of Unicode the tables were generated from. */\n'
        f'inline constexpr std::string_view unicode_version = "{version}";\n',
        enumeration('A code point\'s status in the UTS 46 mapping table.', 'status', [
            ('valid', 'Kept, and valid.'), ('mapped', 'Replaced by its mapping.'),
            ('deviation', 'Kept, or replaced by its mapping in transitional processing.'),
            ('ignored', 'Removed.'), ('disallowed', 'Kept, and an error.')]),
        enumeration('The Bidi_Class values RFC 5893 names, and other for the rest.', 'bidi_class',
                    [(name.lower(), f'Bidi_Class {name}.' if name != 'other' else
                      'Any class RFC 5893 does not name.') for name in BIDI_CLASSES]),
        enumeration('The Joining_Type values RFC 5892 A.1 names, and other for the rest.',
                    'joining_type',
                    [(name.lower(), f'Joining_Type {name}.' if name != 'other' else
                      'Any other joining type.') for name in JOINING_TYPES]),
        enumeration('The scripts RFC 5892 A.4 to A.7 name, and other for the rest.', 'script',
                    [(name.lower(), f'Script {name}.' if name != 'other' else
                      'Any other script.') for name in SCRIPTS]),
        enumeration('A code point\'s NFC_Quick_Check value.', 'quick_check',
                    [('yes', 'Yes: it never changes under NFC.'), ('no', 'No: it never appears '
                      'in NFC.'), ('maybe', 'Maybe: it may compose with what precedes it.')]),
        '// The generator lays the data out, as many values a line as fit 100 columns, and states\n'
        '// each array\'s packing above it: clang-format would put one value on each line.\n'
        '// clang-format off\n',
        array('UTS 46 runs: a first code point shifted left by 32, and the status in bits 0 to 2,'
              ' the NV8 or XV8 mark in bit 3, the mapping\'s length in bits 4 to 8 and its offset'
              ' in uts46_mappings from bit 9.', 'std::uint64_t', 'uts46_runs', uts46),
        array('The code points of every mapping of the UTS 46 table, one after the other.',
              'char32_t', 'uts46_mappings', mappings),
        array('Bidi_Class runs: a first code point shifted left by 8, and a bidi_class.',
              'std::uint32_t', 'bidi_classes', packed(bidi)),
        array('Joining_Type runs: a first code point shifted left by 8, and a joining_type.',
              'std::uint32_t', 'joining_types', packed(joining)),
        array('Script runs: a first code point shifted left by 8, and a script.',
              'std::uint32_t', 'scripts', packed(scripts)),
        array('Canonical_Combining_Class runs: a first code point shifted left by 8, and the'
              ' class.', 'std::uint32_t', 'combining_classes',
              packed(combining_classes(inputs['ucd/extracted/DerivedCombiningClass.txt']))),
        array('General_Category Mark runs: a first code point shifted left by 8, and 1 for a'
              ' mark.', 'std::uint32_t', 'marks', packed(marks(unicode))),
        array('NFC_Quick_Check runs: a first code point shifted left by 8, and a quick_check.',
              'std::uint32_t', 'nfc_quick_checks', packed(quick_checks(normalization))),
        array('The code points that have a canonical decomposition, Hangul syllables aside.',
              'char32_t', 'decomposition_keys', [f'0x{cp:X}' for cp in full]),
        array('Each decomposition_keys entry\'s full decomposition: its offset in'
              ' decomposition_data shifted left by 3, and its length.', 'std::uint32_t',
              'decomposition_slices', slices),
        array('The code points of every full canonical decomposition, one after the other.',
              'char32_t', 'decomposition_data', [f'0x{cp:X}' for cp in decomposition_data]),
        array('Each primary composite\'s pair, its first code point shifted left by 21 and its'
              ' second, Hangul aside.', 'std::uint64_t', 'composition_keys',
              [f'0x{key:X}' for key, _ in composites]),
        array('The primary composite of each composition_keys pair.', 'char32_t',
              'composition_values', [f'0x{cp:X}' for _, cp in composites]),
        '// clang-format on\n',
        '}  // namespace webcpp::idna::detail::tables\n\n'
        '#endif  // WEBCPP_IDNA_DETAIL_TABLES_HPP\n',
    ]
    return '\n'.join(parts)


# Test vectors


def utf8_literal(text: str) -> str:
    """A C++ string_view literal of the UTF-8 of `text` (SV_LITERALS): printable ASCII as it is,
    every other byte as a three-digit octal escape, which never runs into the character after it.
    A lone surrogate is encoded as its three bytes, which no well-formed UTF-8 holds."""
    out = ['"']
    for char in text:
        for byte in char.encode('utf-8', 'surrogatepass'):
            if byte in (0x22, 0x5C):
                out.append('\\' + chr(byte))
            elif 0x20 <= byte < 0x7F:
                out.append(chr(byte))
            else:
                out.append(f'\\{byte:03o}')
    out.append('"sv')
    return ''.join(out)


def u32_literal(code_points: Sequence[int]) -> str:
    """A C++ U""sv literal of the code points (SV_LITERALS), each a universal character name: \\u
    in the Basic Multilingual Plane, \\U beyond it."""
    names = (f'\\u{cp:04X}' if cp <= 0xFFFF else f'\\U{cp:08X}' for cp in code_points)
    return 'U"' + ''.join(names) + '"sv'


def unescape(field: str, where: str) -> str:
    """A field of IdnaTestV2.txt with its \\uXXXX escapes resolved, "" being the empty string."""
    if field == '""':
        return ''
    if '\\' in ESCAPE.sub('', field):
        raise Fault(f'{where}: an escape other than \\uXXXX in {field!r}')
    text = ESCAPE.sub(lambda match: chr(int(match.group(1), 16)), field)
    for first, second in zip(text, text[1:]):
        if 0xD800 <= ord(first) < 0xDC00 and 0xDC00 <= ord(second) < 0xE000:
            raise Fault(f'{where}: the escaped surrogates in {field!r} may be one code point or '
                        'two')
    return text


def has_error(field: str, where: str) -> bool:
    """Whether a status field that is not blank lists a status code, after checking its form."""
    if not (field.startswith('[') and field.endswith(']')):
        raise Fault(f'{where}: the status {field!r} is not a list in brackets')
    codes = [code.strip() for code in field[1:-1].split(',')] if field != '[]' else []
    for code in codes:
        if not STATUS_CODE.fullmatch(code):
            raise Fault(f'{where}: unknown status code {code!r}')
    return bool(codes)


def known_failures(path: Path, version: str, sources: dict[int, str]) -> dict[int, str]:
    """The reason of each line test/known_failures.json lists, by its number, after checking that
    the list is for the pinned version of Unicode and that each entry names a line of
    IdnaTestV2.txt by its number and its source, once, with a reason."""
    try:
        data = json.loads(path.read_text(encoding='utf-8'))
    except ValueError as error:
        raise Fault(f'{path}: {error}') from error
    if (not isinstance(data, dict) or sorted(data) != ['failures', 'unicode']
            or not isinstance(data['failures'], list)):
        raise Fault(f'{path}: not an object of "unicode" and a list "failures"')
    if data['unicode'] != version:
        raise Fault(f'{path}: lists the failures of Unicode {data["unicode"]}, and the pinned '
                    f'files are Unicode {version}')
    found: dict[int, str] = {}
    for entry in data['failures']:
        if (not isinstance(entry, dict) or sorted(entry) != ['line', 'reason', 'source']
                or type(entry['line']) is not int or not isinstance(entry['source'], str)
                or not isinstance(entry['reason'], str) or not entry['reason']):
            raise Fault(f'{path}: {entry!r} is not a line, its source and a reason')
        if sources.get(entry['line']) != entry['source']:
            raise Fault(f'{path}: line {entry["line"]} of IdnaTestV2.txt is not '
                        f'{entry["source"]!r}')
        if entry['line'] in found:
            raise Fault(f'{path}: line {entry["line"]} is listed twice')
        found[entry['line']] = entry['reason']
    return found


def idna_vectors(version: str, text: str, mapping: Mapping, failures_path: Path) -> str:
    """The text of test/generated/idna_test_v2.ipp: one row per line of IdnaTestV2.txt, its blank
    fields resolved as the file's header says."""
    parsed: list[tuple[int, list[str]]] = []
    for number, line in enumerate(text.split('\n'), start=1):
        body = line.split('#', 1)[0]
        if not body.strip():
            continue
        fields = [field.strip() for field in body.split(';')]
        if len(fields) != IDNA_TEST_FIELDS or not fields[0]:
            raise Fault(f'IdnaTestV2.txt line {number}: not a source and six fields')
        parsed.append((number, fields))
    failures = known_failures(failures_path, version,
                              {number: fields[0] for number, fields in parsed})
    out: list[str] = []
    for number, fields in parsed:
        where = f'IdnaTestV2.txt line {number}'
        source = unescape(fields[0], where)
        to_unicode = unescape(fields[1], where) if fields[1] else source
        unicode_error = has_error(fields[2], where) if fields[2] else False
        to_ascii_n = unescape(fields[3], where) if fields[3] else to_unicode
        ascii_n_error = has_error(fields[4], where) if fields[4] else unicode_error
        to_ascii_t = unescape(fields[5], where) if fields[5] else to_ascii_n
        ascii_t_error = has_error(fields[6], where) if fields[6] else ascii_n_error
        strict = not any(mapping.marked[ord(char)] or ord(char) in CONTEXTO
                         for char in to_unicode)
        reason = failures.get(number, '')
        exempt = any(ord(char) in BIDI_CONTROLS
                     for char in source + to_unicode + to_ascii_n + to_ascii_t + reason)
        out.append((f'    // NOLINTNEXTLINE({BIDI_CHECK})\n' if exempt else '')
                   + '    idna_vector{' + ', '.join([
            str(number), utf8_literal(source), utf8_literal(to_unicode),
            str(unicode_error).lower(), utf8_literal(to_ascii_n), str(ascii_n_error).lower(),
            utf8_literal(to_ascii_t), str(ascii_t_error).lower(), str(strict).lower(),
            utf8_literal(reason),
        ]) + '},')
    return vector_file(version, 'IdnaTestV2.txt, a row per test line', 'idna_vector',
                       'idna_vectors', 'test/idna_vectors.hpp', out, [POSITIONAL_ROWS],
                       [SV_LITERALS,
                        'IdnaTestV2.txt holds Bidi embeddings, overrides and isolates on purpose, '
                        'which clang-tidy reads through their escapes: a row that holds one is '
                        f'exempted from {BIDI_CHECK} on its own line.'])


def nfc_vectors(version: str, text: str) -> str:
    """The text of test/generated/nfc_test.ipp: the source, NFC and NFD columns of every line of
    NormalizationTest.txt."""
    out: list[str] = []
    for number, line in enumerate(text.split('\n'), start=1):
        body = line.split('#', 1)[0].strip()
        if not body or body.startswith('@'):
            continue
        fields = body.split(';')
        where = f'NormalizationTest.txt line {number}'
        if len(fields) != NORMALIZATION_TEST_COLUMNS + 1 or fields[-1].strip():
            raise Fault(f'{where}: not {NORMALIZATION_TEST_COLUMNS} columns, each followed by a '
                        'semicolon')
        try:
            columns = [[int(cp, 16) for cp in field.split()] for field in fields[:3]]
        except ValueError as error:
            raise Fault(f'{where}: {error}') from error
        for column in columns:
            if not column or any(cp in SURROGATES or cp > LAST_CODE_POINT for cp in column):
                raise Fault(f'{where}: a column that is not a sequence of scalar values')
        out.append('    nfc_vector{' + ', '.join(u32_literal(column) for column in columns) + '},')
    return vector_file(version, 'NormalizationTest.txt\'s first three columns, a row per line',
                       'nfc_vector', 'nfc_vectors', 'test/nfc_vectors.hpp', out,
                       [POSITIONAL_ROWS], [SV_LITERALS])


def vector_file(version: str, what: str, kind: str, name: str, declared_in: str,
                rows: Sequence[str], exemptions: Sequence[tuple[str, str]],
                notes: Sequence[str]) -> str:
    """A generated file of test vectors: the array `name` of `rows`, whose type `kind` the test
    header `declared_in` declares before it includes the file, with each clang-tidy check of
    `exemptions` turned off around it, after the comment that gives its reason, and the `notes`
    on the rows' own exemptions."""
    checks = ', '.join(check for check, _ in exemptions)
    comments = [reason for _, reason in exemptions] + list(notes)
    reasons = ''.join(textwrap.fill(comment, 100, initial_indent='// ',
                                    subsequent_indent='// ') + '\n' for comment in comments)
    return (NOTICE.format(version=version)
            + f'//\n// {what}.\n// The array {name} of webcpp::test::{kind}, which {declared_in}\n'
            '// declares before it includes this file.\n\n'
            + reasons + f'// NOLINTBEGIN({checks})\n// clang-format off\n'
            f'inline constexpr std::array<{kind}, {len(rows)}> {name}{{\n'
            + '\n'.join(rows) + f'\n}};\n// clang-format on\n// NOLINTEND({checks})\n')


# Main


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    """The command line: the cache directory, and where the outputs go."""
    parser = argparse.ArgumentParser(
        description='Writes idna\'s Unicode tables and its conformance vectors.')
    parser.add_argument('--cache', type=Path, default=None,
                        help='where the pinned files are kept (default: bin/unicode/<version>)')
    parser.add_argument('outputs', type=Path, nargs='*',
                        help=f'where {", ".join(OUTPUTS)} are written, in that order '
                             '(default: their places in the library)')
    arguments = parser.parse_args(argv)
    if arguments.outputs and len(arguments.outputs) != len(OUTPUTS):
        parser.error(f'give no output path, or one for each of {", ".join(OUTPUTS)}')
    return arguments


def generate(cache: Path | None, outputs: Sequence[Path]) -> None:
    """Reads every pinned file, checks the assumptions, and writes every output, or nothing."""
    version, pinned = pinned_files(PINNED)
    cache = cache or SUPERPROJECT / 'bin' / 'unicode' / version
    inputs = {path: fetch(entry, cache) for path, entry in sorted(pinned.items())}
    mapping = Mapping(inputs['idna/IdnaMappingTable.txt'])
    texts = [
        tables_header(version, inputs, mapping),
        idna_vectors(version, inputs['idna/IdnaTestV2.txt'], mapping, KNOWN_FAILURES),
        nfc_vectors(version, inputs['ucd/NormalizationTest.txt']),
    ]
    paths = list(outputs) or [LIBRARY / relative for relative in OUTPUTS]
    for path, text in zip(paths, texts):
        write(path, text.encode('utf-8'))


def main(argv: list[str]) -> int:
    arguments = parse_arguments(argv)
    try:
        generate(arguments.cache, arguments.outputs)
    except (Fault, OSError, KeyError, ValueError) as fault:
        print(f'generate_tables.py: {fault}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
