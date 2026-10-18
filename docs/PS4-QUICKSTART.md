# PS4 0.11 public test release

Download **MickeyDonald-PS4-0.11-Test.pkg** from the
[PS4 0.11 release](https://github.com/NichlasEk/MickeyAndDonaldCollection-PS4/releases/tag/ps4-v0.11).
The automatically generated Source code archives are for developers, not installation.

This is an experimental OpenOrbis/Vulkan Mega Drive emulator for a homebrew-enabled
PS4 with a working PKG installer. It includes five game profiles, a screenshot
carousel, stereo audio, one savestate slot per game and widescreen enhancements.
No ROMs or saves are included. Supply your own compatible ROM files.

## Install and add games

1. Download the PKG. Optionally verify it using the supplied `.pkg.sha256` file
   (`sha256sum -c MickeyDonald-PS4-0.11-Test.pkg.sha256` on Linux).
2. Install it using your PS4's existing homebrew package installer. Title ID is
   `MDCL00001`, version `00.11`. If updating, back up your saves first; 0.11 uses
   the same save format as 0.10. Do not delete your saves to update the app.
3. Rename your raw ROM dumps to the filenames below. Copy them via FTP to
   `/data/mickey-donald/roms/`, or put `mickey-donald/roms/` at the root of a USB
   drive accessible to your PS4. You only need the games you want to play.
4. Launch **Mickey & Donald Collection**. Select a game with the D-pad/left stick
   and press Cross. Triangle rescans if you add files while the app is open.
   The first visit can take longer while game previews are generated locally.

| Filename | Supported game / revision | Bytes | CRC32 |
| --- | --- | ---: | --- |
| `coi.bin` | Castle of Illusion (USA) | 524288 | `BA4E9FD0` |
| `qs.bin` | QuackShot (World, REV 00) | 524288 | `88C8DD94` |
| `woi.bin` | World of Illusion (USA) | 1048576 | `921EBD1C` |
| `lion.bin` | The Lion King (UEJ) | 3145728 | `5696A5BC` |
| `aladdin.bin` | Aladdin Final Cut / ALADDIN-REMIX | 2384052 | `DD66B3FA` |

Use the exact raw versions above. ZIP, SMD and copier headers are not supported
by this PS4 loader. Renaming a different revision does not make it compatible.
HDD is checked first, then USB ports 0 through 3, separately for each game.
A bad HDD copy takes priority over USB: replace/remove that copy if BAD ROM appears.

## Controls and saves

| Context | Control | Action |
| --- | --- | --- |
| Game | D-pad / left stick | Move |
| Game | Square / Cross / Circle | Mega Drive A / B / C |
| Game | Options | Mega Drive Start |
| Game | L1 or touchpad | Open launcher and pause |
| Launcher | R1 | Save the selected currently paused game |
| Launcher | L1 | Load the selected game's save; remain paused |
| Launcher | Touchpad | Resume the paused game without sending a game button |
| Launcher | Cross | Start selected game / resume |
| Launcher | Triangle | Rescan ROM files |
| Game R1 / launcher Square | Cross confirms, Circle cancels | Restart confirmation |

Saves live in `/data/mickey-donald/saves/` as `coi.state`, `qs.state`,
`woi.state`, `lion.state` and `aladdin.state`. There is one slot per game; saving
replaces that slot. There is no autosave. Switching games discards unsaved progress.
Back up this directory via FTP. Saves stay on HDD even when ROMs are on USB.

## What's in this build

- Five-game carousel with locally generated game previews.
- Persistent savestates and restart confirmation.
- Widescreen gameplay, including reconstructed decorative Stampede sides.
- Wider recognised title/options backgrounds with original text proportions.
- Full-width Lion King and Aladdin level starts using decorative scenery outside
  the map. This scenery scrolls away as real map pixels enter view. It does not
  add playable space or duplicate character sprites.
- Aladdin scrolling-edge and Lion King continue-screen fixes.

Some scenes retain their original framing; invalid/transitional layouts keep
protective side masks. One player; upstream PC/Android settings, rewind and
multiplayer UI are not included in this PS4 frontend.

## Test status and feedback

This is a public test build, not a claim that every game has been completed or
that every PS4/firmware combination is supported. Earlier builds ran on a physical
PS4; the latest 0.11 scenery changes have automated host tests and native shadPS4
visual checks, with real-console acceptance still pending. All 21 Lion/Aladdin
level-entry cases passed original-centre, RAM/CPU and save/replay checks. The PKG
was extracted and hash-validated. Emulator benchmarks do not certify console speed.

[Report an issue](https://github.com/NichlasEk/MickeyAndDonaldCollection-PS4/issues)
with your PS4 model, firmware/homebrew environment, game, level and steps to
reproduce. Useful logs: `/data/mickey-donald-ps4.log` and
`/data/mickey-donald-vulkan.log`. Review logs before posting. Do not attach ROMs
or savestates to public issues; states contain game data.

Package SHA-256:
`ecc5c5105ad773e061a0b9fbfec1f4ea5f6695da55571ab15f7971a17bb25d49`

Build source: tag `ps4-v0.11`, commit `5ecae2c3c9b49f1dede782a80e67544c5200c727`.
Original project: [Darhkfox/MickeyAndDonaldCollection](https://github.com/Darhkfox/MickeyAndDonaldCollection).
Code attribution and dependency license notices are included in the package.
