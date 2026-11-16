#!/usr/bin/env python3
"""Reject direct or delay-loaded dynamic MSVC C/C++ runtime dependencies.

Requires pefile (see _tests/requirements.txt). This does not check indirect
imports of operating-system libraries or dynamically constructed LoadLibrary calls.
"""
import pathlib
import sys

import pefile


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: check_msvc_runtime.py <Windows-plugin>')

    path = pathlib.Path(sys.argv[1])
    pe = pefile.PE(str(path))
    names = []
    for directory in ('DIRECTORY_ENTRY_IMPORT', 'DIRECTORY_ENTRY_DELAY_IMPORT'):
        for entry in getattr(pe, directory, []):
            names.append(entry.dll.decode('ascii').lower())
    if not names:
        raise SystemExit(f'{path}: no import table found; cannot verify runtime')
    print('Plugin imports:', ', '.join(sorted(set(names))))
    unwanted = [name for name in names
                if name.startswith(('msvcp140', 'vcruntime140', 'msvcr'))]
    if unwanted:
        raise SystemExit(f'{path}: dynamically linked MSVC runtime: {unwanted}')
    print(f'{path}: no direct/delay-loaded dynamic MSVC runtime dependency')


if __name__ == '__main__':
    main()
