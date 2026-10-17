#!/usr/bin/env python3
"""Measure native offscreen frame work in shadPS4; does not certify display/audio pacing."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mode', choices=('capacity', 'display'), default='capacity')
parser.add_argument('--levels', default='0,4,9', help='zero-based level entries, common to all games')
parser.add_argument('--frames', type=int, default=1200)
parser.add_argument('--timeout', type=int, default=90, help='seconds per level, including startup')
args = parser.parse_args()
if args.mode == "display" and not os.environ.get("DISPLAY"):
    parser.error("display mode requires a desktop X11/XWayland display; Xvfb is unsuitable for pacing acceptance")
levels = [int(x) for x in args.levels.split(',')]
if not levels or any(x < 0 or x >= 28 for x in levels) or not 300 <= args.frames <= 10000 or args.timeout <= 0:
    parser.error('use levels 0..27, 300..10000 frames and a positive timeout')
root = Path(__file__).resolve().parents[1]
emulator = Path(os.environ.get('SHADPS4', ''))
if not emulator.is_file() or not os.access(emulator, os.X_OK):
    parser.error('set SHADPS4 to an executable shadPS4 SDL build')
runtime = root / 'build/runtime'
roms = root / 'build/local-roms'
for path in [runtime / 'eboot.bin', *(roms / (name + '.bin') for name in ('coi', 'qs', 'woi'))]:
    if not path.is_file():
        parser.error(f'missing {path}; build the PS4 runner and prepare local ROMs first')
stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S-%fZ')
output = root / 'build/performance' / stamp
output.mkdir(parents=True)
snapshot = output / 'eboot.bin'
shutil.copyfile(runtime / 'eboot.bin', snapshot)
summary = {'mode': args.mode,
           'scope': ('native CPU, audio mixing and completed GPU render; excludes AudioOut and VideoOut delivery'
                     if args.mode == 'capacity' else 'normal display-loop timing and AudioOut underrun counters; no full playthrough'),
           'physical_ps4_tested': False, 'display_60fps_verified': False,
           'eboot_sha256': hashlib.sha256(snapshot.read_bytes()).hexdigest(),
           'emulator': str(emulator.resolve()), 'frames_per_case': args.frames, 'cases': []}
failed = False
cases = [(level, game) for level in levels for game in
         (('coi', 'qs', 'woi') if args.mode == 'display' else (None,))]
for level, game in cases:
    case = output / f'level-{level}-{game or "all"}'
    stage = case / 'runtime'
    profile = case / 'profile/shadPS4'
    stage.mkdir(parents=True)
    for folder in ('data', 'home/1000/savedata', 'home/1000/trophy', 'home/1000/inputs'):
        (profile / folder).mkdir(parents=True, exist_ok=True)
    for name in ('config.json', 'users.json'):
        source = root / 'build/emulator-profile/shadPS4' / name
        if source.is_file():
            shutil.copyfile(source, profile / name)
    shutil.copyfile(snapshot, stage / 'eboot.bin')
    (stage / 'sce_module').symlink_to(runtime / 'sce_module', target_is_directory=True)
    if game:
        (stage / 'roms').mkdir()
        (stage / 'roms' / (game + '.bin')).symlink_to(roms / (game + '.bin'))
    else:
        (stage / 'roms').symlink_to(roms, target_is_directory=True)
    (stage / 'mdc-test-level.txt').write_text(f'{level}\n')
    if args.mode == 'capacity':
        (stage / 'mdc-benchmark-frames.txt').write_text(f'{args.frames}\n')
    env = dict(os.environ, SDL_VIDEODRIVER='x11', XDG_DATA_HOME=str(case / 'profile'))
    command = [str(emulator.resolve()), '--fullscreen', 'false', str(stage / 'eboot.bin')]
    if not env.get('DISPLAY'):
        command = ['xvfb-run', '-a', '-s', '-screen 0 1280x720x24', *command]
    report = profile / 'data/mickey-donald-ps4.log'
    contents = ''
    with (case / 'emulator.log').open('w') as log:
        process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                if report.exists():
                    contents = report.read_text()
                windows = re.findall(r'^frames=(\d+) ', contents, re.MULTILINE)
                display_done = bool(windows) and int(windows[-1]) >= args.frames
                if 'BENCH complete;' in contents or display_done or process.poll() is not None:
                    break
                time.sleep(0.25)
        finally:
            # Only this invocation's process group; never stop other emulator runs.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    results = []
    for line in contents.splitlines():
        if line.startswith('BENCH game=') or (game and line.startswith('frames=')):
            result = dict(re.findall(r'(\w+)=([^ ]+)', line))
            result['level_entry'] = level
            results.append(result)
            print(f'level={level} {line}', flush=True)
    if game:
        complete = bool(results) and int(results[-1]['frames']) >= args.frames
        passed = complete and all(59 <= float(r['display_fps']) <= 61 and
                                  59 <= float(r['emulation_fps']) <= 61 and
                                  int(r['underrun_frames']) == 0 and
                                  int(r['work_p99_us']) <= 12500 for r in results)
    else:
        complete = 'BENCH complete;' in contents and {r['game'] for r in results} == {'coi', 'qs', 'woi'}
        passed = complete and all(float(r['fps']) >= 80 and int(r['p99_us']) <= 12500 for r in results)
    summary['cases'].append({'level_entry': level, 'game': game, 'complete': complete, 'budget_pass': passed, 'results': results})
    failed |= not passed
    (output / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
print(output / 'results.json')
print('These fixtures do not certify full gameplay or physical PS4 performance. Review max times as well as p99.')
raise SystemExit(1 if failed else 0)
