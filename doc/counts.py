#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)

"""Counts what idna's page says idna holds and webcpp's own counts do not.

tools/doc/counts.py runs this script with idna's directory as its one argument, and gives the page
each `name=<number>` line it prints as an attribute, beside the counts of every library:

* `n-idna-test-v2`, the lines of IdnaTestV2.txt that test/generated/idna_test_v2.ipp holds, one
  row each;
* `n-nfc-vectors`, the lines of NormalizationTest.txt that test/generated/nfc_test.ipp holds.

A count that finds nothing fails, rather than put a zero on the page. A fault is written on the
standard error, and the standard output holds only counts.

Usage: counts.py <library-directory>. Exit 0 printing the counts, 1 naming a fault, 2 on a usage
error.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Each count, the generated file it reads, and how each of its rows starts.
ROWS = {
    'n-idna-test-v2': ('test/generated/idna_test_v2.ipp', '    idna_vector{'),
    'n-nfc-vectors': ('test/generated/nfc_test.ipp', '    nfc_vector{'),
}


class Fault(Exception):
    """A count that cannot be made, said to the user."""


def count(library: Path, relative: str, start: str) -> int:
    """The rows of a generated file: its lines that start with `start`."""
    path = library / relative
    rows = sum(1 for line in path.read_text(encoding='utf-8').splitlines()
               if line.startswith(start))
    if rows == 0:
        raise Fault(f'counts.py: no row starting with {start.strip()!r} in {path}')
    return rows


def main(argv: list[str]) -> int:
    if len(argv) != 1 or not Path(argv[0]).is_dir():
        print('usage: counts.py <library-directory>', file=sys.stderr)
        return 2
    library = Path(argv[0])
    try:
        counted = {name: count(library, relative, start)
                   for name, (relative, start) in ROWS.items()}
    except (Fault, OSError) as fault:
        print(fault, file=sys.stderr)
        return 1
    for name, value in counted.items():
        print(f'{name}={value}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
