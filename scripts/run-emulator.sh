#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
: "${SHADPS4:?Set SHADPS4 to your shadPS4 SDL executable}"
[[ -x "$SHADPS4" ]] || { echo 'SHADPS4 is not executable' >&2; exit 1; }
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
"$root/scripts/build-ps4.sh"
python3 "$root/scripts/prepare-local-roms.py"
python3 - "$root" "$OO_PS4_TOOLCHAIN" <<'PY'
from pathlib import Path
import shutil, sys
root, sdk = map(Path, sys.argv[1:])
runtime = root / 'build/runtime'
(runtime / 'roms').mkdir(exist_ok=True)
(runtime / 'sce_module').mkdir(exist_ok=True)
for source in (root / 'build/local-roms').glob('*.bin'):
    target = runtime / 'roms' / source.name
    if target.exists() or target.is_symlink():
        if target.read_bytes() != source.read_bytes(): raise SystemExit(f'Conflicting local emulator ROM: {target}')
    else: target.symlink_to(source)
for name in ('libc.prx', 'libSceFios2.prx'):
    shutil.copyfile(sdk / 'samples/hello_world/sce_module' / name, runtime / 'sce_module' / name)
profile = root / 'build/emulator-profile/shadPS4'
for folder in ('data', 'home/1000/savedata', 'home/1000/trophy', 'home/1000/inputs'):
    (profile / folder).mkdir(parents=True, exist_ok=True)
PY
launcher=()
if [[ -z ${DISPLAY:-} ]]; then launcher=(xvfb-run -a -s '-screen 0 1280x720x24'); fi
export SDL_VIDEODRIVER=x11
export XDG_DATA_HOME="$root/build/emulator-profile"
# Only this child process is bounded; no other emulator instances are touched.
timeout -k 3s "${EMULATOR_TIMEOUT:-30}s" "${launcher[@]}" "$SHADPS4" --fullscreen false \
  "$root/build/runtime/eboot.bin" > "$root/build/ps4/emulator.log" 2>&1
