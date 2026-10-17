#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
stack=${PS4_STACK:-"$root/../ScummVM-PS4/.tools/opengnm-stack"}
fself=${CREATE_FSELF:-"$HOME/ut99-orbis/build/create-fself-current"}
[[ -x "$fself" ]] || { echo 'Set CREATE_FSELF to the current OpenOrbis create-fself executable' >&2; exit 1; }
cmake -S "$root" -B "$root/build/ps4" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/openorbis.cmake" \
  -DPS4_STACK="$stack" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build/ps4" -j"${JOBS:-8}"
mkdir -p "$root/build/runtime"
"$fself" -in="$root/build/ps4/platform/ps4/MickeyDonaldPS4.elf" \
  -out="$root/build/runtime/MickeyDonaldPS4.oelf" --eboot "$root/build/runtime/eboot.bin" \
  --paid 0x3800000000000011 > "$root/build/ps4/fself.log" 2>&1
readelf -d "$root/build/ps4/platform/ps4/MickeyDonaldPS4.elf" > "$root/build/ps4/imports.txt"
if rg -q 'NEEDED.*(Piglet|Shacc)' "$root/build/ps4/imports.txt"; then
  echo 'Unexpected proprietary graphics import' >&2; exit 1
fi
sha256sum "$root/build/runtime/eboot.bin"
