# Lion King: preserve game timing while rendering side sprites

The Elephant Graveyard timing divergence is fixed in the tested 1200- and
3600-frame sequences. Lion's extra sprites are now rendered by the host without
expanding the game's sprite clipping or adding emulated CPU/DMA work. Both the
ROM clipping patches and the interim VBlank input latch have been removed.
This is a research build; Stampede still uses the original viewport.

## Cause and implementation

The former patches at `0xEB0` and `0xEC2` admitted extra objects to the game's
sprite builder, tile allocator and DMA queue. Initially this moved a controller
poll from host frame 1759 line 253 to frame 1760 line 10, selecting LEFT instead
of LEFT+C. Latching input at VBlank fixed that boundary case, but the 1200-frame
stress sequence still developed a whole-frame phase delay. In the trace the
wide game made an extra sprite-builder call at relative frame 879, and returned
to controller polling a frame later (930 versus 929). Later polls occurred at
the same line 244 but a whole frame apart. Normalizing input alone could not
remove that additional emulated workload.

`lion_sprites.h` replaces the ROM patch with presentation-only observation:

- `0xDCE` starts a pending sprite snapshot, without changing CPU registers/RAM.
- `0xEB0` observes the selected metasprite and positions before original clipping.
  Blink/animation selection has already executed normally. The host reads the
  ROM's 16-byte sprite-piece records, raw tile sources, palette, flip and priority
  bits. It collects a bounded snapshot covering the maximum 40-pixel extension.
- `0x13BE`, immediately after the original SAT DMA, publishes the matching
  pending snapshot. The renderer decodes its tile rows directly into only the
  two side strips, retaining sprite order. Original middle pixels and the
  emulated SAT/VRAM, allocator, DMA queue and controller policy are untouched.

The hooks do not add emulated cycles. The same observation runs at narrow width,
so changing width does not change the simulated game's workload. A 256-piece
bound and ROM-range checks protect the presentation cache. Stampede's separate
perspective mode bypasses this path.

## Savestate persistence

Pending and visible snapshots occupy 6160 bytes of host-only `module_data`.
They are not mapped onto the emulated bus. Core save format v3 adds a bounded
length-prefixed trailer (maximum 64 KiB) after the unchanged machine prefix.
The entire envelope is checked before loading any machine state.

The loader still accepts v2 saves, clearing stale presentation data and allowing
the next original sprite upload to repopulate the cache. Modules without this
extra data continue **writing v2**, preserving the other games' existing format.
New Lion saves require this build or newer; older binaries cannot read v3.
The unit test checks v2 compatibility, v3 round trips, reset, size limits,
truncation and rejection without changing the running machine.

## Regression evidence

```sh
cmake --build build/host -j8
ctest --test-dir build/host --output-on-failure
build/host/lion_input_verify build/local-roms/lion.bin 1200
build/host/lion_input_verify build/local-roms/lion.bin 3600
python3 tools/re/new_games_wide_verify.py --games lion --unpatched-baseline --out build/lion-phase/level-matrix
```

The causal test first deliberately reinstates the old clipping patch as a
negative control. It then runs the same state and changing input through the
new wide path and original clipping. Every rendered middle pixel is compared,
as well as camera positions and controller reads at logical game updates.

| 3600-frame result | Old clipping patch | New host sprites |
| --- | ---: | ---: |
| Completed updates, wide / original | 3261 / 3268 | 3268 / 3268 |
| Differing completed camera updates | 2126 | 0 |
| Controller samples, wide / original | 3430 / 3537 | 3537 / 3537 |
| Differing sampled input values | 1156 | 0 |
| Frames with changed middle pixels | 2939 | 0 |
| Changed middle pixels | 61191359 | 0 |

The 1200-frame run also passes with zero differences. Fresh-session reload
matches both complete core state and all full-width frame images over another
90 frames of changing input. All ten Lion level-entry foreground/pixel/state
cases pass. Six host unit suites and the five ROM audio/disk-state smokes pass;
the original games' frame hashes are unchanged.

Private evidence: `build/lion-phase/isolated-3600.log`, `final-1200.log`, and
`level-matrix/results.json`. The old failing logs remain under `build/lion-timing/`.

## Sprite oracle

```sh
# Reference mdtool library: build source commit 67a4e5f in an isolated checkout.
python3 tools/re/lion_sprite_compare.py --reference-lib /path/to/reference/libmdtool.so --out build/lion-phase/sprite-verified
```

The script uses separate processes for the historical ROM-patched renderer and
the current library. It boots nine side-scrolling levels and compares four
sprite-only snapshots per level. All **36 cases** match both middle and side
pixels exactly. A vanilla SAT-only clone differs by **1908 side pixels** across
these samples, proving the test actually exercises additional sprites.
The historical library also supplies real v2 snapshots loaded by the new build.
Evidence: `build/lion-phase/sprite-verified/results.json` (includes library hashes).

The tests cover these routes and transitions, not every game animation or a
complete playthrough. No ROMs, state files, captures or reference libraries are
published.

## Native runner

Final research EBOOT SHA-256:
`d8ea6dec8a537a02ff5e8c95d2d6ff28cea08016d0a6bce92e7af8ebcc12f754`.
The final shadPS4 menu test starts both additions and saves/loads both slots:
`build/lion-phase/native-menu-final.log`. Visual capture:
`build/ps4/lion-host-sprites-native.png` (isolated Xvfb, not a pacing test).
No new PKG/FTP delivery or physical-console validation was performed.

The final native capacity run passed all three entries (0, 2, 9; 600 frames
each): **256.61–291.99 fps**, worst p99 **4.865 ms**, max **6.177 ms**.
Evidence: `build/performance/20261017T101336-273572Z/results.json`.
Capacity excludes AudioOut/VideoOut delivery; normal display pacing is checked
separately. Emulator results do not establish physical-console performance.

Normal display testing also passed all three entries: settled **60.00 fps**,
zero audio underruns, worst work p99 **9.572 ms**, max **11.740 ms**.
Evidence: `build/performance/20261017T101515-289303Z/results.json`. Both native
performance runs use the final EBOOT hash above.
