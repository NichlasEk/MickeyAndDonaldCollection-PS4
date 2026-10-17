# OpenOrbis / Vulkan bring-up

This branch adds an independent PS4 test runner. The upstream Windows and Android
frontends are unchanged. This is not yet a complete console edition.

## Current behavior

- Reuses the original CPU/VDP/audio cores, the three original and two experimental game modules, Session,
  palette conversion and 16:9 patches.
- Presents a CPU-rendered frame using a host-visible linear RGBA texture and a
  Vulkan textured quad at 1280x720. Preserves square-pixel aspect ratio.
- Keeps one GPU submission in flight, overlapping it with next-frame CPU
  emulation. A fence protects texture, vertex and command-buffer reuse. An
  app-local OpenGNM adapter queues flips across four display buffers and waits
  until a later flip retires an image before rendering into it again. This avoids
  the local ICD's shared semaphore epilogue-buffer reuse bug and redundant idle
  waits. The adapter targets this specific ICD, not arbitrary Vulkan drivers.
- Starts in a five-game launcher: Castle of Illusion, QuackShot, World of Illusion,
  The Lion King and Aladdin Final Cut. The latter two are research-branch additions;
  see [widescreen findings and limitations](re_new_games_wide.md).
  D-pad / left stick selects; Cross starts or resumes, Square requests a confirmed restart,
  Circle returns to the paused game, Triangle rescans ROMs.
- L1 or touchpad opens the launcher during gameplay and pauses the current game.
  Switching to another game discards the previous session; returning to the same
  game resumes it. Pause alone stays in memory. Use the save controls below for persistence.
- Cards show HDD/USB/APP source or MISSING/BAD ROM. Missing or invalid ROMs
  keep the launcher open with a message. Copy ROMs to the paths below and rescan.
- During gameplay R1 opens a restart confirmation (Cross confirms, Circle cancels); D-pad / left stick moves, Square = Mega Drive A,
  Cross = B, Circle = C, Options = Start.
- Native AudioOut: 48 kHz S16 stereo, 512-frame submissions on a dedicated
  consumer thread, an 8192-frame ring and a 3072-frame (64 ms) refill target.
  Audio queue depth controls catch-up after stalls. A two-block deadband
  preserves one game frame per display tick instead of reacting to every
  512-sample AudioOut burst. The first GPU draw completes before audio starts. An underrun outputs silence and re-primes the queue.
  Reset and game switching discard queued audio; already submitted hardware
  blocks may finish. Soft reset also resets the shared resampler.
- In the launcher, R1 saves the selected paused game; L1 loads the selected
  game's slot and stays paused. Cross/Circle resumes after loading. A SAVE badge
  means a file exists, not that it has been validated yet.
- One slot per game at `/data/mickey-donald/saves/{coi,qs,woi}.state`.
  Saving replaces that game's slot; no automatic save on exit or game switching.
  Slots are shared by console users and remain on HDD when ROMs are on USB.
  A bounded, versioned checksum wrapper rejects damaged files. Writes flush and
  sync a temporary file before rename; write failure preserves the old slot.
  Loading validates in a fresh session before replacing the paused game.
  Copy the saves directory via FTP for backup. Compatibility across future core
  revisions is not guaranteed; never commit state files (they contain game RAM).
- Restart requires Cross confirmation; Circle cancels. This applies to both
  gameplay R1 and launcher Square. One player; settings UI remains pending.
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
| `lion.bin` | The Lion King (UEJ) | 3145728 | 5696A5BC |
| `aladdin.bin` | Aladdin Final Cut / ALADDIN-REMIX | 2384052 | DD66B3FA |

On a PS4, copy these files to `/data/mickey-donald/roms/` (HDD, checked first).
For USB fallback, put `mickey-donald/roms/` at the root of the USB drive. The
runner checks `/mnt/usb0/mickey-donald/roms/` through `/mnt/usb3/mickey-donald/roms/`
in order, then `/app0/roms/` as the final development fallback. Priority applies
per game. The selected path is logged; a wrong CRC is rejected rather than run.
The local private delivery folder places the PKG beside `mickey-donald/roms/`.
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
- `dist/MickeyDonald-PS4-0.05-Test.pkg`, SHA-256 sidecar and JSON manifest

Title ID: `MDCL00001`; version: `00.05`. Distinct from ScummVM and UT99.
The package contains a newly generated 512x512 RGB8 dashboard icon and copyright/license notices for
this project, OpenOrbis, OpenGNM, vulkan-ps4 and opengnm-psbc.
Icon: `res/ps4_icon0.png`; generation prompt: `res/ps4_icon_prompt.txt`.

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

## Performance gate before hardware testing

Target: stable 60 Hz output, with capacity for at least 80 fps (12.5 ms frame
work). Keep emulation at the original game speed. A fast core-only benchmark
is insufficient; inspect CPU, audio mixing, completed GPU work, presentation,
frame repeats/catch-up and AudioOut underruns separately.

The native runner reports rolling 300-frame rates, p95/p99/max work, individual
render stages, frame repeats and extra catch-up frames. Work excludes the
intentional wait for display ownership but includes GPU fence waits. These are
wall-clock application timings, not GPU timestamp queries or unique scanout
counts. Inspect maxima as well as percentile values.

Use the reproducible harness after building and preparing ROMs:

```sh
# Capacity: 120 warmup frames, then 1200 timed frames per game/level entry.
# CPU, audio mixing, texture upload, full 1280x720 draw and GPU completion;
# a private render target excludes AudioOut delivery and VideoOut refresh.
DISPLAY= SHADPS4=/path/to/shadps4 python3 scripts/benchmark-ps4.py \
  --mode capacity --levels 0,4,9 --frames 1200

# Pacing: run from a real desktop X11/XWayland session. Each game uses its
# own staging directory and profile. This opens and closes test windows.
SHADPS4=/path/to/shadps4 python3 scripts/benchmark-ps4.py \
  --mode display --levels 0,4,9 --frames 1800
```

The harness snapshots the EBOOT and records its hash. Results, logs, fixtures
and private ROM links stay in ignored `build/performance/<timestamp>/`.
A capacity case requires >=80 fps and p99 <=12.5 ms. A display case requires
59–61 fps for both the display loop and emulation, zero cumulative underrun
frames and work p99 <=12.5 ms in every window. These short fixtures are a
minimum gate, not proof of a full playthrough or physical-console performance.
No synthetic inputs are sent to the user's desktop.

The earlier 50–54 fps ceiling came from the Xvfb presentation setup: even a
ROM-free color pattern was limited there, while the same binary reached 60 fps
on the desktop. Do not use Xvfb to accept/reject normal display pacing, and do
not raise the emulated vblank frequency to inflate results.

A nine-case capacity run (entries 0, 4, 9 in all three games; 1200 timed frames
per case) measured 274–330 fps and 3.9–9.6 ms p99. Maximum individual times
reached 24.2 ms, so this is not a claim that every frame meets the budget.
Several unrelated emulator workloads were active on the host. The separate
30-second first-level display tests reached about 60 fps; an initial Castle
underrun motivated moving GPU warmup before AudioOut startup.

## Validation boundary

The first candidate cross-compiles and links with OpenOrbis. Both existing CPU
suites pass on Linux, and all three supplied ROMs pass the host smoke test.
Both SPIR-V shaders pass `spirv-val --target-env vulkan1.0`.
The ROM-free package passes `PkgTool.Core pkg_validate`; all 11 extracted payloads
match their manifest hashes and the extracted EBOOT matches the tested runtime.

The packaged 0.03 EBOOT matches the final nine-case desktop display run
(entries 0, 4, 9 in all three games, 600 frames per case). Every case passed:
60.00–60.49 display-loop fps, zero underrun frames, work p99 4.36–9.91 ms,
and maximum individual work time 14.81 ms. The first window includes priming;
steady windows recorded 0–1 repeated frames and no extra catch-up frames.
The 512-sample AudioOut quantum no longer forces continual paired skip/catch-up
updates. Host simulation tests cover this timing regression and stall recovery.
These are short mostly idle level fixtures; they do not cover every gameplay
situation. Earlier capacity runs had larger host scheduling outliers.

Nonzero stereo output was captured separately for all three games before this
pacing refinement. The mock AudioOut and presentation tests do not validate
firmware behavior. Four host suites and all three ROM smoke tests pass.

The 0.03 PKG and separate ROM folder were delivered by FTP with full SHA-256
readback. The user subsequently confirmed successful physical PS4 operation
with Castle of Illusion. This is not a measured console performance result or
verification of the other two games. Version 0.04 adds the launcher and awaits
physical-console testing. Its PKG and sidecars have been delivered to `/data/pkg/`
with full SHA-256 readback; existing ROMs were verified separately. Its native shadPS4 UI test starts all three games,
returns to the menu, resumes, restarts and rescans. Missing and invalid ROM
handling is checked separately. Version 0.05 adds persistent save/load and restart confirmation. Host tests
cover failed writes, corrupt/truncated files and fresh-session replay for all
three ROMs. Native save/reload after process restart is tested separately.
Second-player input and settings remain pending; 0.05 awaits physical testing.

## Public source hygiene

Run `python3 scripts/audit-public-history.py` after committing and before pushing.
It scans all reachable Git blobs for ROM headers, game-data/build paths and
common credential signatures. Review the staged file list too; credential
patterns are heuristic. Original ROMs, canonical ROM copies, captures, saves,
SDK libraries and PKGs stay local. The PS4 packaging script uses an explicit
payload allowlist, independently of `.gitignore`.
