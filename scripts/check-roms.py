#!/usr/bin/env python3
"""Identify local raw ROMs without copying or publishing game data."""
import argparse
from pathlib import Path
import zlib
EXPECTED = {
    0xBA4E9FD0: ('coi', 524288, 'Castle of Illusion'),
    0x88C8DD94: ('qs', 524288, 'QuackShot'),
    0x921EBD1C: ('woi', 1048576, 'World of Illusion'),
}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('directory', nargs='?', type=Path, default=Path(__file__).resolve().parents[1] / 'roms')
a = p.parse_args()
found = set()
for path in sorted(a.directory.iterdir()):
    if not path.is_file() or path.suffix.lower() not in ('.bin', '.md', '.gen'):
        continue
    if path.stat().st_size > 1048576:
        print(f'UNSUPPORTED {path.name}: expected a raw ROM of 512 KiB or 1 MiB')
        continue
    data = path.read_bytes()
    crc = zlib.crc32(data)
    spec = EXPECTED.get(crc)
    if spec and len(data) == spec[1]:
        found.add(spec[0])
        print(f'OK {spec[0]} {spec[2]} CRC32={crc:08X} file={path.name}')
    else:
        print(f'WRONG VERSION/FORMAT {path.name}: size={len(data)} CRC32={crc:08X}')
for crc, (key, size, title) in EXPECTED.items():
    if key not in found:
        print(f'MISSING {key}: {title}, {size} bytes, CRC32={crc:08X}')
raise SystemExit(0 if len(found) == 3 else 1)
