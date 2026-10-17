# Latest local feature: menu widescreen backgrounds

Recognised title/options backgrounds now extend without stretching text or
characters. Lion: title/options scenery. Aladdin: title/options palace and sky.
Castle: title mountains. QuackShot: selection/options checkerboard. World:
plain backdrop around its title stage. Black screens and framed/storybook
artwork retain their original presentation; this is not an all-screen unlock.
See `docs/re_new_games_wide.md` for exact coverage and reproduction commands.

Ten menu fixture cases pass centre/replay/mode-switch tests; full state and
centre hashes match 0.09 for ten scenes and five 1200-frame START transitions.
Six host suites, Lion continue and Stampede/Pride Rock regressions pass.
Native EBOOT SHA-256:
`3c9a7041dd07348e09a03652346d69d455db1ec0ebbec5feabab56d11ab76e11`.
Native shadPS4 saved-state load/resume captures of both title menus were
visually checked: `build/menu-wide/native-menu/{lion,aladdin}/title.png`.
Private evidence: `build/menu-wide/`. No save format change.
No new PKG/FTP delivery yet; installed/uploaded 0.09 below does not include this.
Physical-console menu acceptance remains pending.

# Latest delivery: 0.09 Stampede widescreen

Uploaded to `192.168.32.229:2121` at
`/data/pkg/MickeyDonald-PS4-0.09-Test.pkg` with manifest and checksum sidecar.
Includes reconstructed decorative Stampede sides and all previous graphics fixes.
Pride Rock widescreen was reverified; existing saves remain compatible.
Package SHA-256: `ef9b1b2f91c7151247dd811ceb0b389bd2e50249e52b7571795723d2af80d509`.
EBOOT SHA-256: `bb6f3ba0c96c26b4aff0d438917fbd9a303baf5496b336701dba7a2d00b4aede`.
Native release build and PkgTool validation pass. All nine extracted filesystem
payloads match the manifest, including the current EBOOT. All three FTP uploads
were read back and SHA-256 verified before rename. No ROMs are bundled.
Evidence: `build/release-009-ftp.json`, `build/release-009-package.log`.
This delivery supersedes the local-only Stampede status below. Installation and
physical-console acceptance of Stampede remain pending.

# Latest local feature: Stampede reconstructed widescreen

Stampede now presents 400x224 without stretching its original 256-pixel centre.
Its source contains three pre-rendered canyon cels, not a wider world map.
The added sides reflect original canyon-wall artwork and repeat the sky cel.
This is reconstructed decorative scenery, NOT additional playable level area.
H32/H40 transitions are recentered to avoid a one-frame width jump at death.
Pride Rock already had widescreen and was explicitly reverified as requested.

`lion_stampede_verify` passes 1200 frames each for entries 3 and 9: original
centre, emulated RAM and CPU PC match narrow mode, including death transition;
state and full-image replay pass. All ten Lion entry tests pass in
`build/lion-stampede/matrix-final/results.json`. Six host suites and the continue
regression pass. Native visual captures exist for both levels under
`build/lion-stampede/native-3.png` and `native-9.png`.
Capacity run: 208.39 / 241.12 fps, p99 6104 / 8387 us (before a diagnostic-only
zero-extension guard correction). Normal display run uses the final EBOOT;
see `build/lion-stampede/display.log` for 60 Hz and audio evidence.
Final EBOOT SHA-256:
`4a1a2c8d801c1710a78bda099c187da8509f6618cfda7b482a9928572b6bf830`.
No new PKG or FTP delivery; uploaded 0.08 below still has original-width
Stampede. Physical-console acceptance remains pending.

# Latest delivery: 0.08 menu title and both graphics fixes

Main menu now reads MICKEY & DONALD / ALADDIN & THE LION KING.
Version 0.08 includes Aladdin's active-scanout fix and Lion's continue-screen
classification fix. Uploaded to `192.168.32.229:2121` at
`/data/pkg/MickeyDonald-PS4-0.08-Test.pkg` with manifest and checksum sidecar.
Package SHA-256: `f119095e59b4bb46b6fab94843d80060ea8e5e087ee10c6c003cdb5b3866c740`.
EBOOT SHA-256: `959cebb4f1d00652eb0aeb42d54289c9e780fee5b540f4b299f5c9691cc28cc6`.
Native build, menu layout inspection and PkgTool validation pass. All nine
extracted filesystem payloads match the manifest and current EBOOT. All three
uploads were read back and SHA-256 verified before rename. No ROMs are bundled.
Evidence: `build/release-008-ftp.json`, `build/release-008-package.log`.
Existing saves remain compatible. Installation and physical acceptance pending.
The historical local-only notes below are superseded by this delivery.

# Latest local fix: Lion continue-screen side corruption

The private user save reproduces CONTINUE? incorrectly classified as gameplay.
CD40, the old map pointers and VDP layout survive the transition. The object
slot addressed by CD36 is reused for a menu object with descriptor 08EA90;
normal side-scrolling cameras have descriptor 08B7EE at object+4. All nine
side-scrolling level snapshots share that descriptor; Stampede has 1B51B6 and
retains its existing original-width fallback. `playing()` now checks the camera
object descriptor before enabling map/sprite extension rendering.

`lion_continue_verify ROM SESSION_STATE` exercises 90 clean menu frames, chooses
YES and sees 841 gameplay frames restored; all 1200 original centre images match
the narrow renderer, and state replay passes. Fixture is a Session state without
the outer PS4 store header and stays private under `build/lion-continue/`.
The 1200-frame Lion timing test also passes with zero camera/input/centre changes.
All ten Lion level-entry checks and six host suites pass. Native shadPS4
save-load/resume capture confirms clean continue-screen gutters
(`build/lion-continue/native-fixed/fixed.png`). Native build EBOOT SHA-256:
`24db4fbd95e49d14087899f51aba30886bac206dba3b4cac00c5dd0849cf5d81`.
This fix is not yet packaged or FTP-delivered. Installed 0.07 lacks this fix and
the Aladdin active-scanout fix below. Physical verification remains pending.

# Latest local fix: Aladdin clipped foreground extensions

User supplied an exact native save showing chopped coals and bazaar artwork.
Tracing the real scanout shows FF7E2E is FF at line zero but clears by line 24
while the game main loop executes. The per-line playing() guard then skipped
foreground reconstruction for the remaining scanlines, exposing stale streamed
nametable columns. A frozen redraw concealed the bug by using the end-of-frame
flag throughout. The hook now uses the gameplay decision latched at line zero.
CPU, ROM patches, camera, original centre and save format are unchanged.

New regression: `tools/re/aladdin_scanout_verify.py` compares real foreground
scanout with frozen redraw during a settled stationary scene. Reproducible from
ROM boot or a private core state. User state: old 90899 edge pixel differences
over 60 frames, fixed zero; centre zero in both. Boot fixture: old 186709 edge
pixel differences, fixed zero, centre zero. Existing 11-level Aladdin matrix
(120 frames each) and all six host suites pass. Native saved-state load/resume
and capture pass in isolated shadPS4 (`build/aladdin-edge/native-fixed/`).
Private evidence and user state remain under ignored `build/aladdin-edge/`.
EBOOT SHA-256: `34fbf6f9486986b9e06561f5f90d00c049bbd4ed8834ef6562c60ffc0a575358`.
This fix is not packaged/FTP-delivered or physically verified. Version string
still 0.07; uploaded 0.07 below predates this fix. User's visible emulator also
still runs its earlier copied executable; do not mistake it for the fixed build.

# Latest delivery: 0.07 centred level boundaries

Package uploaded to `192.168.32.229:2121` at
`/data/pkg/MickeyDonald-PS4-0.07-Test.pkg` with manifest and checksum sidecar.
SHA-256: `34044ccccca68fb078f67361e1324a89b536e92768b3f069ebb32a4aef5d4907`.
EBOOT SHA-256: `0da8316f5e66cf9e9976fe5ee86d7c9f5d2e2134fa9f8d90f9642f17da90c631`.
Only the version log changed after the boundary-fix build; native rebuild and
PkgTool validation passed. All nine extracted filesystem payloads match the
manifest and runtime EBOOT. All three FTP uploads were read back and verified
before rename. Evidence: `build/centred-edges-007-ftp.json`.
All five ROMs were already on HDD; no ROMs are bundled or retransferred.
Existing saves are retained and compatible. Physical installation/test pending.

# Latest local fix: centred level boundaries

Lion and Aladdin now report equal left/right presentation masks using the larger
level-boundary mask. This hides the opposite extension temporarily instead of
leaving the image visibly lopsided. The Session's existing easing opens both
sides together when the camera scrolls into the level. Original central pixels,
ROM patches and camera/game timing are unchanged. Stampede stays original width.
The two games use thumbnail cache v2 to regenerate their launch-card images.

All 21 level-entry checks passed (120 frames each), including unchanged centre
pixels, extension oracle, state replay and symmetric masks.
Both 300-frame Session-level checks passed: matching black side bands at the
start, full widescreen after scrolling. Captures and private check are under
`build/centred-edges/`. Six host CTest suites pass. PS4 EBOOT built:
`216664974024e291b532ea5a20fbc768f6fa19702189d1880c25c6eceb2007ad`.
This fix is included in the 0.07 delivery above; physical verification is pending.
The previous 0.06 package below still has asymmetric boundary masks.

# Latest delivery: 0.06 carousel package

Uploaded to `192.168.32.229:2121` as
`/data/pkg/MickeyDonald-PS4-0.06-Test.pkg`, with manifest and SHA-256 sidecar.
Package SHA-256: `8b8a9070338da8aeccdad3815120ee0762048fe143d69b4087cce860e2e59747`.
PkgTool validation passed; all nine extracted filesystem payloads match the
manifest, including the tested carousel EBOOT. Metadata/icon are covered by
package validation. Package contains no ROMs.
Lion and Aladdin were missing remotely and transferred separately to
`/data/mickey-donald/roms/{lion,aladdin}.bin`. All five uploaded files were read
back over FTP and SHA-256 verified before renaming into place.
Evidence: `build/carousel-ftp.json`, `build/carousel-package.log`.
Installation and physical-console testing remain pending. First launch generates
local thumbnail caches and can take longer. Source changes remain local.

# Latest: carousel launcher (local review)

The launcher now renders at 800x448 with a wrapping, animated horizontal
carousel, game screenshots, per-game accents and save/paused status. Left/right
and the previous up/down controls browse. Existing play, save/load and restart
confirmation controls are retained. Presenter texture capacity is 1024x512;
gameplay resolution is unchanged.

First launch generates five thumbnails using isolated Sessions and level entry
zero from the verified local ROMs. This can take time; a preparation screen is
shown per game. Pixel-only 320x180 caches live under
`/data/mickey-donald/previews/<id>-v1.bgra`, outside save slots. Missing or
wrong-sized caches are regenerated; unavailable ROMs get a placeholder. No game
images or ROMs are embedded in source or package. Delete the cache to regenerate.

Native build and six existing host CTest suites pass. Isolated shadPS4 verified
all five generated previews, wrap from Castle to Aladdin and back, and Lion plus
Aladdin start/save/load/resume. Evidence: `build/carousel-native.log`,
`build/ps4/launcher-carousel-*.png`. Visual inspection passed. A separate visible
instance was opened for user review (`build/carousel-visible/pid`).
EBOOT SHA-256: `911f678e0dfaecd7874f9a6f86b4ea122ae130500331f428e171cdd9816d5027`.
No new PKG, FTP delivery or physical PS4 verification. Changes currently local.

# Latest: Lion side sprites no longer alter emulated timing

The remaining Elephant Graveyard timing issue is fixed in the 1200- and
3600-frame regressions. See [implementation and evidence](re_lion_input_timing.md).
Lion's ROM clipping patches and interim input latch are removed. Read-only CPU
hooks capture metasprites into a serialized host-only cache and render the side
strips, without adding game CPU/DMA work. Stampede remains original width.

The 3600-frame comparison has zero camera/input/middle-pixel differences.
36 sprite-oracle cases match the old visual output and exercise 1908 extra side
pixels. All ten Lion level entries pass. Six host suites and all five ROM smokes
pass; new-state replay includes all frame pixels after a fresh-session reload.
Legacy v2 saves load; modules without extra presentation data still write v2.
New Lion saves use v3 and need this build or newer.

Final EBOOT SHA-256:
`d8ea6dec8a537a02ff5e8c95d2d6ff28cea08016d0a6bce92e7af8ebcc12f754`.
Native menu/start/save/load test: `build/lion-phase/native-menu-final.log`.
Final native capacity (entries 0/2/9): 256.61–291.99 fps, p99 <= 4865 us;
`build/performance/20261017T101336-273572Z/results.json`. Normal display: settled
60.00 fps, zero audio underruns, work p99 <= 9572 us;
`build/performance/20261017T101515-289303Z/results.json`.
No new package/FTP or hardware test. Branch: `ps4/widescreen-aladdin-lion`.
The following is historical evidence; earlier failure status and hashes are
superseded by the timing investigation above.

# Earlier research branch — 2026-10-17

Current branch: `ps4/widescreen-aladdin-lion`, based on stable `2151947`.
See [widescreen research](re_new_games_wide.md) for exact ROM identities,
implementation, reproducer and limitations. Five-game PS4 menu and separate
save slots are implemented. Both new games passed shadPS4 menu/start/save/load.
All 21 level entries booted; Stampede intentionally retains original width.
The unpatched comparison has one known Elephant Graveyard trajectory difference
caused by widened sprite submission. Rendering-width checks use identical
patches to isolate this variable; see the research document before acceptance. All five host suites and all five ROM smokes pass,
including fresh-session disk-state replay. Original three smoke hashes unchanged.

Version 0.06 is a research executable only; no 0.06 PKG/FTP delivery was made.
Stable 0.05 and the default branch remain separate. ROMs and evidence stay local.
Native capacity tests: `build/performance/20261017T093445-714838Z/results.json`;
all six cases (entries 0/4/9 in both games) passed: 280.54–328.41 fps,
worst p99 4861 us, worst single frame 6622 us. This excludes AudioOut/VideoOut.
Private host oracle: `build/widescreen-r3/results.json`; UI log:
`build/sidequest/native-menu.log`. No new physical-console validation.

Normal display tests also passed all six cases at 60.00–60.42 fps, zero audio
underruns, work p99 <= 9491 us; evidence:
`build/performance/20261017T093621-282837Z/results.json`.
Performance EBOOT SHA-256: `086b4a38931d8606ac4df0ee61251ed1c4478cf54b7f9d301d4dc51d5073fc23`.
Final EBOOT adds the Aladdin 16-pixel left guard-column mask:
`222cde217ac96f6308c36b5aab1a363789fd29e7b57f1d5d6a5fc88bcacc3298`.
All 21 same-patch rendering-width cases passed in r3; final Aladdin rerun is
`build/widescreen-r4/results.json` (11/11 pass). Final Aladdin display retest:
`build/performance/20261017T094211-597763Z/results.json`, PASS at 60.02 fps
settled, zero underruns, p99 <= 7963 us.

The following is historical stable-port evidence, not the research release:

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
