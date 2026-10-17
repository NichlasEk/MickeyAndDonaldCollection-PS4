# OpenOrbis / Vulkan bring-up

This branch adds an independent PS4 test runner. The upstream Windows and Android
frontends are unchanged. This is not yet a complete console edition.

## Current behavior

- Reuses the original CPU/VDP/audio cores, all three game modules, Session,
  palette conversion and 16:9 patches.
- Presents a CPU-rendered frame using a host-visible linear RGBA texture and a
  Vulkan textured quad at 1280x720. Preserves square-pixel aspect ratio.
- Keeps one GPU submission in flight, overlapping it with next-frame CPU
  emulation. A fence protects texture, vertex and command-buffer reuse. An
  app-local OpenGNM adapter queues flips across four display buffers and waits
  until a later flip retires an image before rendering into it again. This avoids
  the local ICD's shared semaphore epilogue-buffer reuse bug and redundant idle
  waits. The adapter targets this specific ICD, not arbitrary Vulkan drivers.
- Auto-starts the first available supported game: Castle, QuackShot, World.
- L1 cycles available games; R1 resets the current game. D-pad / left stick moves,
  Square = Mega Drive A, Cross = B, Circle = C, Options = Start.
- Without ROMs, shows an animated color test pattern; L1 retries ROM loading.
- Native AudioOut: 48 kHz S16 stereo, 512-frame submissions on a dedicated
  consumer thread, an 8192-frame ring and a 3072-frame (64 ms) refill target.
  Audio consumption clocks emulation; display stalls cause frame skipping rather
  than slowed game audio. An underrun outputs silence and re-primes the queue.
  Reset and game switching discard queued audio; already submitted hardware
  blocks may finish. Soft reset also resets the shared resampler.
- One player, no launcher/menu or user save-state UI yet.
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
- `dist/MickeyDonald-PS4-0.02-Test.pkg`, SHA-256 sidecar and JSON manifest

Title ID: `MDCL00001`; version: `00.02`. Distinct from ScummVM and UT99.
The package contains a neutral generated icon and copyright/license notices for
this project, OpenOrbis, OpenGNM, vulkan-ps4 and opengnm-psbc.

## Emulator run

Set `SHADPS4` to a shadPS4 SDL executable and run `scripts/run-emulator.sh`.
This uses an isolated profile under `build/emulator-profile`, local-only ROM
links and SDK sample runtime modules. `EMULATOR_TIMEOUT` defaults to 30 seconds;
a timeout exit status of 124 is expected for the continuously running app.
With no X11 display, the script uses Xvfb. Logs go to `build/ps4/emulator.log`.

For local level tests only, put a zero-based level number in
`build/runtime/mdc-test-level.txt`. Remove it for normal startup. To retain
per-draw Vulkan tracing, create `build/runtime/mdc-vulkan-trace.txt`; otherwise
only startup tracing is enabled. Neither marker enters the package allowlist.

## Performance target and measurements

Target: 60 displayed fps, with capacity for about 80 fps (12.5 ms frame work),
without speeding up the original game. **This target has not been met or verified.**
The app reports rolling 300-frame display-loop and emulation rates, p95/max
CPU-plus-render work, separate core/render p95 and average display ownership
wait. Render work includes GPU fence waits. These are application timings, not
GPU timestamp queries or a count of unique frames actually scanned out.

In the latest shadPS4/Xvfb first-level run, display-loop throughput was roughly
50–54 fps (previous baseline about 30). Emulation remained about 60 fps. Core
batches were 6.5–7.6 ms p95, while render work including waits was 13–18 ms p95;
combined work was about 18–21 ms p95. The 12.5 ms total budget is therefore not
established. The host also had other emulator workloads; this is not a clean
hardware benchmark. Do not stop unrelated processes to improve the numbers.

## Validation boundary

The first candidate cross-compiles and links with OpenOrbis. Both existing CPU
suites pass on Linux, and all three supplied ROMs pass the host smoke test.
Both SPIR-V shaders pass `spirv-val --target-env vulkan1.0`.
The ROM-free package passes `PkgTool.Core pkg_validate`; all 11 extracted payloads
match their manifest hashes and the extracted EBOOT matches the tested runtime.

shadPS4 initializes native Vulkan, accepts controller input and switches all
three games. The latest local level fixture reached each game's first level,
with game imagery and nonzero captured stereo output through an isolated audio
sink. Castle and QuackShot had zero recorded underrun frames in that run; World
had 99 missing sample frames (about 2.1 ms) before re-priming. Earlier runs also
had occasional underruns; audio stability under stalls still needs testing.
The four host suites cover both CPU cores, the actual AudioOut consumer against
a gated mock, and presentation ownership/error handling against a mock timeline.
These mocks do not validate firmware behavior.

No physical PS4 execution, audible console test, FTP transfer, installation or
full playthrough has been performed. 60 fps with 80 fps capacity remains an open
acceptance requirement. Second-player input, launcher/settings and filesystem
save UI are also pending.

## Public source hygiene

Run `python3 scripts/audit-public-history.py` after committing and before pushing.
It scans all reachable Git blobs for ROM headers, game-data/build paths and
common credential signatures. Review the staged file list too; credential
patterns are heuristic. Original ROMs, canonical ROM copies, captures, saves,
SDK libraries and PKGs stay local. The PS4 packaging script uses an explicit
payload allowlist, independently of `.gitignore`.
