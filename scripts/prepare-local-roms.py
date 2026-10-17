#!/usr/bin/env python3
"""Create ignored, local-only canonical ROM copies for tests / manual PS4 transfer."""
import hashlib
from pathlib import Path
import zlib
root = Path(__file__).resolve().parents[1]
expected = {0xBA4E9FD0: ('coi', 524288), 0x88C8DD94: ('qs', 524288), 0x921EBD1C: ('woi', 1048576)}
out = root / 'build/local-roms'
out.mkdir(parents=True, exist_ok=True)
found = set()
for source in sorted((root / 'roms').iterdir()):
    if not source.is_file() or source.suffix.lower() not in ('.bin', '.md', '.gen'):
        continue
    if source.stat().st_size not in (524288, 1048576): continue
    data = source.read_bytes()
    spec = expected.get(zlib.crc32(data))
    if not spec or len(data) != spec[1]: continue
    key = spec[0]
    target = out / (key + '.bin')
    # Never follow an existing symlink when writing local copies.
    if target.is_symlink(): raise SystemExit(f'Refusing symlink destination: {target}')
    if target.exists() and target.read_bytes() != data: raise SystemExit(f'Conflicting ROM: {target}')
    if not target.exists(): target.write_bytes(data)
    assert hashlib.sha256(target.read_bytes()).digest() == hashlib.sha256(data).digest()
    found.add(key)
    print(f'OK {key}: {target} (local only; excluded from packages and Git)')
if len(found) != 3: print('Missing: ' + ', '.join(sorted({'coi', 'qs', 'woi'} - found)))
raise SystemExit(0 if found else 1)
