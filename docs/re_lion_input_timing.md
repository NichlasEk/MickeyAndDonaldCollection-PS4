# Lion King sprite workload and input timing

Follow-up to the Elephant Graveyard divergence in the initial widescreen branch.
The short-case input bug is fixed; long-run timing equivalence is **not** established.
This remains a research build, not a new console release.

## Cause established by tracing

The wider clipping bounds admit extra objects to the sprite builder at `0xDCA`,
including tile-cache allocation and queued graphics uploads. Lion reads the
controller at `0xB66` after its VBlank work. Breakpoint records now include the
VDP scanline, making the host-frame boundary visible.

In the failing sequence, the original-width game polls at frame 1759, line 253.
The wide game polls at frame 1760, line 10, after the host has supplied its next
input sample. These reads belong to the same logical game update but receive
LEFT (`0x04`) versus LEFT+C (`0x24`). The camera subsequently diverges. This is
not evidence of a broken map renderer or corrupted collision data.

Lion now latches both controller ports at VBlank, retaining that sample across
line zero. The policy applies at both widths. Pending and latched input are
stored in the existing serialized `module_state[0..1]`; configuration changes
and loading a save do not discard them. The other game modules and the core's
default input policy are unchanged. This deliberately places Lion's host-input
sampling point at VBlank instead of the start of the host frame.

## Causal regression

```sh
cmake --build build/host -j8
build/host/lion_input_verify build/local-roms/lion.bin
python3 tools/re/new_games_wide_verify.py --games lion --unpatched-baseline --out build/lion-timing/latched-baseline
```

The C++ test boots Elephant Graveyard, forks the same state into wide and
original-clip runners, and changes direction/jump input for 240 frames. It first
disables the new latch as a negative control, then repeats with it enabled.
Both runners use the same sprite-limit policy; the ROM clipping patches differ.

| 240-frame comparison | Old input policy | VBlank latch |
| --- | ---: | ---: |
| Different polled input samples | 1 | 0 |
| Different camera positions at completed game updates | 58 | 0 |
| Frames with changed centre pixels | 78 | 0 |
| Total changed centre pixels | 1597640 | 0 |

The first differing old input is sample 149; the first differing completed
update is 150. Mid-host-frame RAM snapshots can differ temporarily even when
completed updates and displayed pixels agree, so those counts are reported
separately rather than treated as gameplay divergence.

Fresh-session save/load with another 90 frames of changing input passes exactly.
All ten Lion level-entry cases also pass the 240-frame unpatched-clip baseline,
foreground extension oracle and state replay. Private records:
`build/lion-timing/causal-240-final.log` and
`build/lion-timing/latched-baseline/results.json`.

## Remaining long-run limit

```sh
build/host/lion_input_verify build/local-roms/lion.bin 1200
```

This stricter stress test intentionally still exits nonzero. With the latch,
the first differing completed camera update moves from 150 to 1005; differing
updates fall from 643 to 106. The first differing input sample moves from 149
to 658. That later sample is polled at **the same scanline 244 but a full host
frame apart** (2570 versus 2569), around a scripted direction/jump transition.
There are 1123 versus 1124 completed sprite-build updates by the end of the run.

The simple frame-boundary latch cannot remove a whole-frame phase difference.
Extra emulated sprite/upload workload remains the leading explanation; the
exact transition causing this later delay needs isolation before claiming full
timing equivalence. A strict fix must keep additional presentation work from
delaying the original game, rather than just alter the test's input sequence or
hide mismatches. Do not describe the 1200-frame test as passing.
Evidence: `build/lion-timing/causal-1200-detail.log`. Earlier traces are retained
under the same ignored directory. No ROMs, state files or captures are published.

## Native runner

The patched PS4 EBOOT is
`95f75a345aec1efa8245198078b312c70ee25f6d8455b8c32f19b87809c28b62` (SHA-256).
Elephant Graveyard's normal shadPS4 display run passed 600 frames: settled
60.00 fps, zero audio underruns, worst work p99 8.082 ms. Evidence:
`build/performance/20261017T095103-405342Z/results.json`.
This measures host-side PS4 execution and presentation, not the original game's
logical-update timing equivalence, full gameplay, or physical PS4 performance.

The separate native capacity test passed at 266.79 fps, p99 4.734 ms, max
7.008 ms (600 frames, same EBOOT). Evidence:
`build/performance/20261017T095401-448487Z/results.json`. Five host suites and
Lion's 600-frame audio/fresh-session disk-state smoke also passed.
