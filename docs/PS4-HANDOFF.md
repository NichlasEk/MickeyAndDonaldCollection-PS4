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

Current test PKG SHA-256:
`dist/MickeyDonald-PS4-0.02-Test.pkg`:
`225c8451449892c037b4741043b5940ca19216185db3ad5c18e6467baa0735ca`.
The previous 0.01 package is preserved. Version 0.02 passed pkg_validate;
all 11 payload hashes matched after extraction, including the tested EBOOT.
Always recheck the sidecar after rebuilding; the generated manifest contains
archive hashes and per-file package hashes.

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

Latest first-level evidence: `build/ps4/emulator-audio-level.log`,
`build/ps4/*-level-emulator.png`, `build/ps4/audio-capture-results.json` and
private `build/ps4/audio-*.pcm`. Each game produced 8.192 seconds of captured
nonzero stereo audio. Capture used a private null sink; only the owned test
process's stream was routed/unmuted, then its mute state was restored. Other
applications and existing emulator processes were not altered.

Measured rolling display-loop rate was approximately 50–54 fps, emulation about
60 fps. Castle/QuackShot recorded no underruns; World recorded 99 missing sample
frames (2.1 ms). Work p95 was about 18–21 ms, core batches about 6.5–7.6 ms and
render work including fence waits about 13–18 ms. Several unrelated emulator
workloads were active on the host, so treat these as diagnostic measurements.

The required 60 fps with roughly 80 fps capacity is NOT achieved/verified.
Next: measure GPU completion and flip pacing on the physical PS4, then resolve
the remaining presentation stalls and audio underruns under load. Do not claim
that core-only timings prove overall headroom. Full playthrough, UI and
second-player support remain pending. No physical-console execution, FTP
transfer or installation has been performed.

The local `mdc-test-level.txt` fixture was removed after testing; normal boot is
restored. Diagnostic markers, ROMs and screenshots are excluded by the package
allowlist. Use `scripts/audit-public-history.py` after commits and before pushes.
