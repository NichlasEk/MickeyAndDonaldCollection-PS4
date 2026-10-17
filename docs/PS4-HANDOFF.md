# PS4 attempt — 2026-10-17

Repository moved out of ScummVM-PS4 to a sibling `MickeyAndDonaldCollection/`.
Local branch: `ps4/openorbis-vulkan`; upstream base: `055e930`.
Public fork: `https://github.com/NichlasEk/MickeyAndDonaldCollection-PS4`.
`origin` points to that fork; `upstream` points to Darhkfox/MickeyAndDonaldCollection.
The PS4 branch is the fork's default branch. Only source and documentation are
published; ROMs, private captures and packages remain local.

Read [PS4.md](PS4.md) for build commands, controls, paths and limitations.
Do not rebuild or overwrite neighboring ScummVM/UT99 projects.

All three local ROM CRCs match. Original filenames remain untouched in `roms/`;
canonical read-back-verified copies are in `build/local-roms/`. Both locations
are Git-ignored. No ROM data is included in the PKG.

Evidence is under `build/ps4/` and the isolated
`build/emulator-profile/shadPS4/`. `emulator.log` records the original invalid
PM4 packet with semaphores. `emulator-fence.log` and `emulator-visual.log` record
the corrected single-fence presentation. The earlier serial-submit-marker test
is exploratory only; the final runtime does not require that marker.
The screenshots are private generated artifacts, not tracked source assets.

Host smoke: 600 frames per game, frame bounds valid, nonzero resampled audio and
exact core-state replay after save/load. Hashes over rendered frames:

- coi: `1cd6b3cc4c3925f8`
- qs: `305fb6d5df12fe24`
- woi: `dbcb1327d3d11e80`

Current test PKG: `dist/MickeyDonald-PS4-0.03-Test.pkg` (19,726,336 bytes),
SHA-256 `ef08801120242dfb80f0c9385c416f4938ff859a527ce9bbee68497c8b655a9b`.
EBOOT SHA-256 `c06bd855d0b434ec6013e87ba84584d83bc274d7e208a1f680ff0c06715b9c50`.
Earlier 0.01/0.02 packages are preserved. Version 0.03 passed pkg_validate;
all 11 payload hashes matched after extraction, including the tested EBOOT.
The new generated icon is `res/ps4_icon0.png`, RGB8 512x512, with its prompt in
`res/ps4_icon_prompt.txt`. The original artwork is private under `build/artwork/`.

Private delivery folder: `dist/MickeyDonald-PS4-0.03/`, containing the PKG,
checksums, manifest, README and separate `mickey-donald/roms/`.
FTP delivery to the user's console completed with full readback of eight files:
PKG/sidecars in `/data/pkg/`; ROMs and instructions in `/data/mickey-donald/`.
Receipt: `build/ps4/ftp-receipt.json` (private endpoint details).
No installation or physical execution was performed.

Native graphics dependency checkouts (locally patched; revisions alone are not
sufficient to reproduce the archives):

- vulkan-ps4: `f4d940b723771b3fe637e8ac0704bb5d6ab19622`
- opengnm: `c51243f333a1a964894903da080a508f7af4d91f`
- opengnm-psbc: `a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92`

AudioOut now uses 48 kHz S16 stereo, a dedicated consumer thread and a bounded
FIFO. Audio consumption clocks the emulation. The host test exercises the actual
consumer with a gated mock. All four host suites and the three ROM smoke tests
pass. The app-local presentation adapter queues flips and protects scanout
ownership. GPU fences protect mapped resources and the shared ICD epilogue.
The CPU can emulate the next frame while the prior GPU draw completes.

Performance evidence:

- Capacity: `build/performance/20261017T082847-260842Z/results.json`.
  Nine cases, 1200 timed frames each after 120 warmup frames. Native core,
  resampling, 1280x720 private render target and GPU completion: 274–330 fps,
  p99 3.9–9.6 ms, maximum 24.2 ms. AudioOut/VideoOut delivery excluded. This
  run preceded the final display pacing refinement; the offscreen path is unchanged.
- Final display: `build/performance/20261017T083847-358943Z/results.json`.
  Nine cases (entries 0, 4, 9 in coi/qs/woi), 600 frames each, real desktop
  X11/XWayland output. The report's EBOOT hash matches the package exactly.
  All passed: 60.00–60.49 loop fps, zero underruns, work p99 4.36–9.91 ms,
  maximum 14.81 ms. Steady windows: 0–1 repeated frames, no catch-up frames.
- Xvfb capped even the ROM-free pattern at ~50 fps; the same binary reached
  60 fps on the desktop. Do not treat that Xvfb ceiling as core throughput.
- The first GPU draw now completes before AudioOut startup to avoid startup
  starvation during lazy shader compilation. A two-block audio queue deadband
  avoids unnecessary paired duplicate/catch-up frames at steady 60 Hz.
- Earlier isolated captures in `build/ps4/audio-*.pcm` contain actual nonzero
  stereo output for all three games. Other application streams were untouched.

Run `scripts/benchmark-ps4.py --help` for reproducible capacity/display cases.
Each run snapshots its binary and isolates its profile, ROM links and fixtures.
Normal `build/runtime/` has no test markers. Generated data never enters Git
or the PKG. The runner searches HDD first, USB0..3 second, and /app0 last;
see PS4.md for exact paths. The selected ROM path is logged.

The minimum emulator p99 budget now passes, but these short fixtures are not a
full playthrough or a physical-PS4 guarantee. Longer interactive/worst-case
coverage is still required; no console installation/execution has occurred.
Full UI and second-player support are later work. Do not overwrite neighboring
projects or stop unrelated emulator processes. Audit public history after
committing and before pushing; keep ROMs, the delivery folder and receipts local.
