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

Previous baseline PKG: `dist/MickeyDonald-PS4-0.03-Test.pkg` (19,726,336 bytes),
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
No installation or execution was performed by the agent. The user subsequently
confirmed 0.03 works well on physical PS4, with Castle of Illusion.

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
coverage is still required. User-confirmed Castle operation on 0.03 does not
verify all three games or the new 0.04 launcher. Settings/save-state UI and
second-player support are later work. Do not overwrite neighboring
projects or stop unrelated emulator processes. Audit public history after
committing and before pushing; keep ROMs, the delivery folder and receipts local.

## Version 0.04 launcher

Default boot now shows all three games with ROM availability/source. Cross starts
or resumes, Square starts fresh, Triangle rescans; L1/touchpad opens the launcher
and pauses gameplay. Circle returns to the active game. A game switch discards
the old session. Persistent save/load controls are not implemented.

Native shadPS4 UI validation: all three games start, menu return/resume works,
World restarts, rescan works; missing/invalid ROMs stay in the menu. Screenshots
and logs are private under `build/ps4/launcher-*`. Xvfb is used only for these UI
checks, not performance acceptance. All four host suites pass. Gameplay pacing
is unchanged; the 0.03 performance reports above retain their original binary
identity. Explicit local test-level fixtures still bypass the launcher.

Current PKG: `dist/MickeyDonald-PS4-0.04-Test.pkg` (19,791,872 bytes).
PKG SHA-256: `44afaeaebc5e8294b48852432d24ef4c19d32ea851aeba849d65535867d797a5`.
EBOOT SHA-256: `c402f385654c3356af67a2783b6bba5f53df43d98bc031accafb72a7ad05953c`.
`pkg_validate` and extraction/readback of all 11 allowlisted payloads passed.
The package contains no ROMs or test fixtures. Private side-by-side delivery
folder: `dist/MickeyDonald-PS4-0.04/`.

FTP 0.04 delivery completed: `/data/pkg/MickeyDonald-PS4-0.04-Test.pkg` with
checksum/manifest. The three existing HDD ROMs were read back and matched;
versioned instructions/checksums were added without replacing 0.03 artifacts.
All eight files were SHA-256 verified. Receipt:
`build/ps4/ftp-004-receipt.json`. Installation and physical 0.04 test remain
for the user; FTP delivery does not install the package.

## Version 0.05 persistent saves and restart confirmation

One HDD slot per game: `/data/mickey-donald/saves/{coi,qs,woi}.state`.
Launcher R1 saves the selected paused game, L1 loads the selected slot and stays
paused. SAVE badges indicate file presence. Saving overwrites that one slot;
no autosave. States stay private and are not included in packages or Git.
Checksummed, bounded wrapper; temporary file + flush/fsync + rename. Failed
loads leave the current session intact by validating in a separate Session.
Gameplay R1 and launcher Square now require Cross confirmation; Circle cancels.

Five host suites pass, including disk roundtrip, corruption/truncation rejection
and previous-slot preservation after write failure. All three ROM smoke tests
also replay identically after disk load into a fresh Session. The 0.03 physical
Castle report remains the only console evidence; 0.05 requires user testing.

Native shadPS4 evidence: `build/ps4/savestate-native-save.log` saves all three
slots; `savestate-native-load.log` loads all three after process exit/relaunch,
then resumes play. Both restart cancellation and confirmation were exercised.
Screenshots: `build/ps4/launcher-*-saved.png`, `launcher-*-loaded.png`.
Current package: `dist/MickeyDonald-PS4-0.05-Test.pkg`.
SHA-256: `9c9dae8b694a4d48badebf89f080833b0948d1f5618c34225db47c9f06dfbf32`.
EBOOT: `d29ab3cd559d1394949956eed3f817412f39e9751f65bbdb16ef25e022a20b1d`.
Package validation and readback matched all 11 payloads and the tested EBOOT;
no ROMs, states or test markers are packaged. This test uses Xvfb for UI only,
not performance acceptance. Gameplay pacing is unchanged.

FTP 0.05 delivery completed with SHA-256 readback of package, sidecars,
versioned instructions/checksums and all three existing ROMs. Receipt:
`build/ps4/ftp-005-receipt.json`. Ready for the user to install from
`/data/pkg/MickeyDonald-PS4-0.05-Test.pkg`. Existing console saves were not
accessed or overwritten by deployment. Physical 0.05 validation remains pending.
