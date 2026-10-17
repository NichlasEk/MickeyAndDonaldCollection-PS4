# OpenOrbis / Vulkan bring-up

This branch adds an independent PS4 test runner. The upstream Windows and Android
frontends are unchanged. This is not yet a complete console edition.

## Current behavior

- Reuses the original CPU/VDP/audio cores, all three game modules, Session,
  palette conversion and 16:9 patches.
- Presents a CPU-rendered frame using a host-visible linear RGBA texture and a
  Vulkan textured quad at 1280x720. Preserves square-pixel aspect ratio.
- Uses a single frame in flight with CPU fence waits. This avoids the local ICD's
  shared semaphore epilogue-buffer reuse path, which produced an invalid PM4
  packet in the initial emulator test.
- Auto-starts the first available supported game: Castle, QuackShot, World.
- L1 cycles available games; R1 resets the current game. D-pad / left stick moves,
  Square = Mega Drive A, Cross = B, Circle = C, Options = Start.
- Without ROMs, shows an animated color test pattern; L1 retries ROM loading.
- One player, no launcher/menu, no audible output, no user save-state UI yet.
  The original audio generation and save-state code remain in the shared core.
- GPU objects are allocated once and owned until process exit for this diagnostic.
  GPU failures are logged; resources are not freed after a device-lost error.

## ROMs (private, local only)

Put original raw `.bin`, `.md` or `.gen` dumps in `roms/`. Names do not matter to
these host tools. Unlike the original full frontend, the PS4 bring-up loader does
not yet accept ZIP, SMD or copier headers.

```
python3 scripts/check-roms.py
python3 scripts/prepare-local-roms.py
```

The second command makes verified copies under ignored `build/local-roms/`:

| File | Game | Size | CRC32 |
| --- | --- | --- | --- |
| `coi.bin` | Castle of Illusion | 524288 | BA4E9FD0 |
| `qs.bin` | QuackShot | 524288 | 88C8DD94 |
| `woi.bin` | World of Illusion | 1048576 | 921EBD1C |

On a PS4, copy these files to `/data/mickey-donald/roms/`. The runtime also checks
`/app0/roms/` as a fallback, useful for a local emulator staging directory.
The package script never bundles ROMs. ROMs, states and generated screenshots
are excluded from Git. The source game's licensing does not grant rights to
redistribute Sega/Disney game content.

Logs: `/data/mickey-donald-ps4.log` and `/data/mickey-donald-vulkan.log`.

## Host validation

```
cmake -S . -B build/host -DMDC_HEADLESS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/host -j8
ctest --test-dir build/host --output-on-failure
build/host/mdc_smoke build/local-roms/coi.bin
build/host/mdc_smoke build/local-roms/qs.bin
build/host/mdc_smoke build/local-roms/woi.bin
```

The smoke runner identifies the exact ROM, executes 600 frames with widescreen,
checks frame bounds and nonzero resampled audio, and compares core state after a
save/load/replay with the same input. It does not prove full-game accuracy.

## PS4 build

Requires OpenOrbis, clang/lld, CMake, Python, glslangValidator, a current
OpenOrbis create-fself, and an already built native OpenGNM Vulkan stack.

```
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
export PS4_STACK=/path/to/opengnm-stack
export CREATE_FSELF=/path/to/current/create-fself
scripts/build-ps4.sh
python3 scripts/package-ps4.py
```

`PS4_STACK` must contain `vulkan-ps4/libvulkan_ps4.a`, `opengnm/libopengnm.a`,
`opengnm-psbc/libpsbc.orbis.a` and `Vulkan-Headers/include/`.
This initial checkout reuses the locally patched stack from a sibling
`ScummVM-PS4/.tools/opengnm-stack` by default. It does not modify that stack.
A clean upstream stack has not been verified as a substitute. The generated
package manifest records the exact archive hashes used.

Outputs:

- `build/ps4/platform/ps4/MickeyDonaldPS4.elf`
- `build/runtime/eboot.bin`
- `dist/MickeyDonald-PS4-0.01-Test.pkg`, SHA-256 sidecar and JSON manifest

Title ID: `MDCL00001`; version: `00.01`. Distinct from ScummVM and UT99.
The package contains a neutral generated icon and copyright/license notices for
this project, OpenOrbis, OpenGNM, vulkan-ps4 and opengnm-psbc.

## Emulator run

Set `SHADPS4` to a shadPS4 SDL executable and run `scripts/run-emulator.sh`.
This uses an isolated profile under `build/emulator-profile`, local-only ROM
links and SDK sample runtime modules. `EMULATOR_TIMEOUT` defaults to 30 seconds;
a timeout exit status of 124 is expected for the continuously running app.
With no X11 display, the script uses Xvfb. Logs go to `build/ps4/emulator.log`.

## Validation boundary

The first candidate cross-compiles and links with OpenOrbis. Both existing CPU
suites pass on Linux, and all three supplied ROMs pass the host smoke test.
Both SPIR-V shaders pass `spirv-val --target-env vulkan1.0`.
The ROM-free package passes `PkgTool.Core pkg_validate`; all 11 extracted payloads
match their manifest hashes and the extracted EBOOT matches the tested runtime.

shadPS4 initializes native Vulkan, loads all three games, shows game imagery and
accepts L1 game switching and Options/Start through its keyboard-to-pad mapping.
Captured screens include Castle's options menu and QuackShot's story sequence;
these are startup/menu evidence, not full gameplay validation. The measured
loop is approximately 30 fps in this emulator setup, below the NTSC core's
roughly 60 fps. Do not interpret this as physical-console performance or full
playthrough validation. Audio output, second-player input, menus, filesystem
save UI and a physical PS4 test remain pending.
