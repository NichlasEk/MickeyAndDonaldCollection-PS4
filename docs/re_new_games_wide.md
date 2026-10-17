# Lion King and Aladdin Final Cut widescreen research

Branch: `ps4/widescreen-aladdin-lion`, based on stable PS4 version 0.05.
The source identifies itself as 0.06 research. No new package or console delivery
is implied by these tests. Original PC/Android launchers still have three games;
the PS4 launcher has five, with independent persistent save slots for each.

Lion now renders side sprites on the host without additional emulated CPU/DMA
work. The former Elephant Graveyard divergence is fixed in both 1200- and
3600-frame comparisons; see [the timing investigation](re_lion_input_timing.md).

## Method

The normal H40 image grows from 320×224 to 400×224 (40 pixels per side).
The rendering hook writes only extension pixels; game VRAM, collision maps and
camera logic are not rewritten. Lion uses a presentation-only metasprite cache
to preserve the original emulated workload.
Each module reconstructs foreground extension pixels from the game's decoded
RAM map and ROM metatile definitions. The existing VDP pattern renderer handles
palette, flips and priority. Background rendering uses the existing VDP path.
Aladdin still uses checked ROM sprite-clipping patches. Lion observes the
original sprite builder and draws the extension directly; source ROM files
are untouched.

The map camera must be aligned with the actual latched HScroll/VSRAM phase.
RAM camera coordinates can already describe the next frame. An initial oracle
run exposed this mismatch; `wide_map.h` fixes it and captures boundary masks for
the rendered frame rather than the next camera position. Masks hide pixels
beyond the level bounds. Menus and unsupported modes retain the original viewport.

## Exact supported versions and layout

| Game | Bytes | CRC32 | Key addresses |
| --- | --- | --- | --- |
| Lion King UEJ | 3145728 | 5696A5BC | camera pointer FFCD36; map FFCD22; definitions FFCD2A; row bytes FFCD1E; rows FFCD20 |
| Aladdin Final Cut / ALADDIN-REMIX | 2384052 | DD66B3FA | camera FF7E06/08; dimensions FF7DC8/CC; definitions FF7DCE; row pointers FF9896 |

Lion King metatiles are 16×16: map offsets select four tile words from ROM.
The former clipping patches at 0xEB0 and 0xEC2 were removed. Hooks at 0xDCE,
0xEB0 and 0x13BE collect and publish a presentation-only sprite snapshot.
The diagnostic level entry uses the normal dispatcher at 0x1D765A.

Aladdin uses a row-pointer table with 64-byte spacing and 16×16 metatiles.
Clipping comparisons at 0x1D6110 and 0x1D6152 extend the right bound; its
existing left clipping and object retention margins cover the extension.
The diagnostic level entry hooks 0x1D4A96. Dungeon's foreground is on plane B
(FFF695); Rug Ride uses an already-populated repeating plane, so it bypasses
map reconstruction. Narrow Abu arenas are masked to their actual map width.
Aladdin also masks the first 16 map pixels: this guard column contains edge
artifacts outside the intended visible area, observed in native captures.

**Lion King's Stampede now has a reconstructed 400x224 presentation.** Its
original 256-pixel centre is unchanged; reflected canyon-wall artwork extends
the sides, and the sky cel repeats. These are decorative extensions, not new
level geometry or a wider playable area. See the Stampede section below. Other cutscenes/bonus modes, sprite spawning
near the new edges and complete playthroughs still need broader manual coverage.
The sampled pixel oracle verifies foreground reconstruction, not all gameplay
or all background/sprite behavior.

## Reproducible checks

Use your own exact ROMs locally; no ROMs, state files or screenshots belong in Git.

```sh
python3 scripts/check-roms.py
python3 scripts/prepare-local-roms.py
cmake -S . -B build/host -DMDC_HEADLESS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/host -j8
ctest --test-dir build/host --output-on-failure
python3 tools/re/new_games_wide_verify.py --out build/widescreen-r3
```

The regression boots all 10 Lion King and 11 Aladdin entries, runs 240 scripted
frames each, and checks three snapshots per entry. At each snapshot it compares
the centre with an extension-disabled instance using the **same sprite patches
and sprite-limit policy**, so this isolates rendering width from game execution. A separate frozen vanilla VDP
has its nametable populated from decoded map data and supplies an independent
rendering oracle for extension pixels. Stampede skips the extension oracle.
Each case also checks exact core-state replay over 30 frames after save/load.

All 21 rendering-width cases passed: zero changed centre pixels, zero wrong
checked extension pixels, and matching state replay.
Private evidence is in `build/widescreen-r3/results.json`; the earlier failing
scroll-phase test is retained in `build/widescreen/results.json`.

The separate unpatched-baseline run (`build/widescreen-r2/results.json`) passed
20/21 cases. Lion King Elephant Graveyard diverged in the centre after frame 151;
at frame 239 it differed by 26393 pixels, while the extension oracle and state
replay still passed. Isolating the variables reproduced zero centre differences
with identical patches on both runs, and zero with patches removed from both.
Changing sprite limits alone did not remove the divergence. This establishes
the ROM sprite-submission patch as the trigger; altered emulated timing is the
working explanation, not proven full-game equivalence. Preserve this finding:
manual playthrough and game-timing review remain necessary before calling this
production-ready. This is historical evidence from before the input-latch fix. The current
Lion `--unpatched-baseline` and the dedicated 1200/3600-frame timing tests
now pass after replacing the sprite patch with host rendering. See the follow-up
for current results; the following performance figures refer to earlier builds.

Five host unit suites and all five ROM smoke tests passed. The isolated
shadPS4 UI test started both additions and saved/loaded both slots; evidence:
`build/sidequest/native-menu.log`, `build/ps4/launcher-five-games.png`.

For native performance, set SHADPS4 to the local SDL executable and run:

```sh
python3 scripts/benchmark-ps4.py --games lion,aladdin --levels 0,4,9 --frames 600 --mode capacity
# On a real desktop display, not Xvfb:
python3 scripts/benchmark-ps4.py --games lion,aladdin --levels 0,4,9 --frames 600 --mode display
```

Capacity includes emulation, audio mixing and completed GPU work, but excludes
AudioOut/VideoOut delivery. Display testing exercises normal pacing separately.
Neither establishes physical PS4 performance or a complete playthrough.

Native capacity run (600 frames × six cases, entries 0/4/9): **280.54–328.41 fps**,
worst p99 **4.861 ms**, worst individual frame **6.622 ms**. All six passed the
80 fps / 12.5 ms p99 target. Evidence:
`build/performance/20261017T093445-714838Z/results.json`.

Normal display run: all six cases passed, 60.00–60.42 displayed fps,
zero audio underrun frames, worst work p99 9.491 ms, worst frame 13.918 ms.
The first 300-frame windows include startup/catch-up; settled windows are
60.00–60.01 fps. Evidence:
`build/performance/20261017T093621-282837Z/results.json`.
Both native runs used EBOOT SHA-256
`086b4a38931d8606ac4df0ee61251ed1c4478cf54b7f9d301d4dc51d5073fc23`.

The subsequent Aladdin guard-column mask is the only runtime change after those
performance runs. Final research EBOOT SHA-256:
`222cde217ac96f6308c36b5aab1a363789fd29e7b57f1d5d6a5fc88bcacc3298`.
Aladdin's final 11-entry matrix passed in `build/widescreen-r4/results.json`.
A final native Aladdin entry-0 display retest also passed: 60.02 fps settled,
zero underruns, worst p99 7.963 ms;
`build/performance/20261017T094211-597763Z/results.json`.
Native visual captures (private): `build/ps4/lion-widescreen-native.png` and
`build/ps4/aladdin-widescreen-native.png`. These Xvfb captures are visual evidence,
not pacing measurements.

## Real scanout regression: Aladdin busy flag

The frozen map oracle missed an active-scanout issue: FF7E2E clears during the
game main loop. Rechecking it on each line stopped extension reconstruction
partway through a frame. Aladdin now latches gameplay at line zero for the full
frame. `python3 tools/re/aladdin_scanout_verify.py` exercises CPU-driven scanout
and a stationary foreground redraw, from a reproducible ROM boot sequence.
An optional `--state` accepts a private raw core snapshot. The former library
fails this regression, while the corrected library has zero differing pixels.

## Lion continue-screen classification

Continue reuses gameplay's camera slot and map memory while leaving CD40 and
the VDP layout unchanged. Only an object with the normal camera descriptor
08B7EE at +4 now enables Lion's side extensions. Menu objects such as 08EA90
fall back to the original viewport. The private-fixture host target
`lion_continue_verify ROM SESSION_STATE` verifies clean gutters, unchanged centre,
YES returning to gameplay widescreen and save-state replay.

## Stampede presentation and Pride Rock verification

Stampede's H32 nametable is 128x32 tiles: three pre-rendered 256-pixel canyon
animation cels, followed by a 256-pixel sky cel. Naive extension crosses into
a different cel. `lion_stampede.h` samples reflected wall artwork from the
current cel and wraps the sky within its own cel. Centre pixels and sprites
are unscaled. At the 16:9 setting, 72 native pixels are added per side, making
400x224. Only presentation state changes; ROM, CPU, collisions and object spawn
limits remain original. H32/H40 transitions recenter the framebuffer once to
avoid a 464-pixel flash at death. The existing mode-off diagnostic remains
untouched. This is deliberately described as reconstructed scenery, not
previously unseen original artwork.

`build/host/lion_stampede_verify build/local-roms/lion.bin` covers both Stampede
and Pride Rock for 1200 frames each with changing controls, compares the entire
original centre, all emulated RAM and CPU PC against normal width, and verifies
30-frame state plus full-image replay. Stampede includes a death/mode transition.
The general level matrix uses the actual centre offset rather than assuming
40 pixels for all modes; the Stampede map oracle is replaced by these dedicated
presentation checks because this scene has no side-scrolling level map.

## Recognised menu backgrounds (local successor to 0.09)

`menu_wide.h` recognises background nametable signatures and reconstructs only
extension pixels in the scanline hook. This is decorative presentation, not
additional ROM artwork or gameplay. The original centre, logo, text, menu
cursor and character sprites keep their positions and proportions.

Supported scenes:

- Lion King title and options: reflect the Pride Rock and sky planes; suppress
  sprites outside the original viewport. Continue retains its original black
  background and is never classified as gameplay.
- Aladdin title and options: reflect the palace/starfield plane B; clear plane A
  and sprites in the extensions so logos and text never repeat.
- Castle of Illusion title: reflect the castle/mountains on plane B. Its black
  options screen remains black.
- QuackShot selection and options: repeat the outer 16-pixel checker pattern on
  plane B. The framed parchment title keeps its existing presentation.
- World of Illusion title: extend the plain backdrop around the framed stage;
  keep the stage, logo and characters intact. Storybook pages are unchanged.

Only recognised signatures enable `FrameInfo::menu_widescreen`; other screens
retain their protective masks. Recognition resets every frame and is latched
at scanline zero, including after loading a state. Older modules reserve
`VDP::hook_flags` bits 8..11 for this host-only policy, preserving HUD bit 0.
The signature is FNV-1a over the first 4096 bytes of the relevant nametable;
layout checks disambiguate the table. No graphics, ROM or savestate data is
embedded in source. Sprite/plane pixels are copied before palette composition,
so the game's own raster effects and fades continue to apply.

Validation (private inputs and captures under `build/menu-wide/`):

- `menu_wide_verify ROM CORE_STATE EXPECT_MENU OUTPUT_PREFIX`: ten menu cases,
  180 frames each; original centre exact, safe fallback masks, visible sides
  on recognised scenes, state/full-image replay and 4:3/wide toggles. Lion and
  Aladdin additionally compare all RAM and CPU PC against narrow rendering.
- `menu_wide_trace` compiled against release commit `939ca88` and this change:
  ten scenes x 180 frames and five START transitions x 1200 frames have identical
  full emulated-state and original-centre hashes at the same widescreen setting.
  This separate baseline is necessary because the older games already patch
  CPU instructions in wide mode. `baseline-compare.log`, `transition-compare.log`.
- Six CTest suites, Lion's 1200-frame continue regression and Stampede/Pride Rock
  regressions pass. Logs: `ctest.log`, `continue.log`, `stampede.log`.
- Native OpenOrbis build passes. Both title menus were loaded/resumed and
  visually checked in shadPS4: `native-menu/{lion,aladdin}/title.png`,
  `native-menu.log`. Resume used touchpad so the resume button did not also
  select START inside the game. Physical-console testing is still separate.

Example standalone baseline trace build (use baseline paths for all includes
and both archives when compiling the baseline executable):

```sh
c++ -std=c++17 -O2 -I. -Isrc tools/re/menu_wide_trace.cpp \
  build/host/libmdc_session.a build/host/libmdcore.a -o build/menu-wide/trace-current
build/menu-wide/trace-current ROM CORE_STATE
build/menu-wide/trace-current ROM CORE_STATE --start
```

The optional third argument exercises START after 60 frames, then runs to 1200.
The tool prints hashes only. All ROMs, fixtures, captures and binaries stay local.
