#!/usr/bin/env python3
"""Check that a file is a 64-bit AMD64 Windows PE executable or DLL."""
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
data = path.read_bytes()
if len(data) < 64 or data[:2] != b'MZ':
    raise SystemExit(f'{path}: not a DOS/PE file')
pe = struct.unpack_from('<I', data, 0x3c)[0]
if pe + 26 > len(data) or data[pe:pe + 4] != b'PE\0\0':
    raise SystemExit(f'{path}: invalid PE header')
machine = struct.unpack_from('<H', data, pe + 4)[0]
magic = struct.unpack_from('<H', data, pe + 24)[0]
if machine != 0x8664 or magic != 0x20b:
    raise SystemExit(f'{path}: expected x64 PE32+, got machine={machine:#x} magic={magic:#x}')
print(f'{path}: Windows x64 PE32+ OK')
