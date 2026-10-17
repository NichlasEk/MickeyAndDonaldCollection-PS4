#!/usr/bin/env python3
"""Private-ROM regression for Aladdin's mid-frame busy flag.
Run with MDTOOL_DLL pointing at a host mdtool build. No captures are published.
The game is stationary before each check, so foreground scanout must match a
frozen redraw. This exercises CPU execution between scanlines, unlike the map
oracle that only compares frozen renderers. Optional --state is a raw core state.
"""
import argparse, ctypes as C, json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from mdt import ModuleMD, _dll
_dll.mdt_render_only.argtypes = [C.c_void_p]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom', type=Path, default=ROOT/'build/local-roms/aladdin.bin')
p.add_argument('--state', type=Path)
p.add_argument('--out', type=Path, default=ROOT/'build/aladdin-scanout/results.json')
a = p.parse_args()
m = ModuleMD(str(a.rom), 'aladdin'); m.configure(40)
if a.state:
    m.load(a.state.read_bytes()); m.configure(40)
else:
    assert m.start_level(0)
    m.hold('RIGHT'); m.mrun(60)
    m.hold(0); m.mrun(60)
m.debug_layers(1); m.hold(0)
cases = []
for f in range(60):
    m.mrun(1); w,h,actual = m.frame_raw()
    _dll.mdt_render_only(m.h); rw,rh,expected = m.frame_raw()
    assert (w,h)==(rw,rh)==(400,224)
    centre = sum(actual[y*w+x]!=expected[y*w+x] for y in range(h) for x in range(40,360))
    sides = sum(actual[y*w+x]!=expected[y*w+x] for y in range(h) for x in (*range(40),*range(360,400)))
    cases.append({'frame':f,'centre_differences':centre,'side_differences':sides})
result = {'passed':all(not c['centre_differences'] and not c['side_differences'] for c in cases),
          'frames':len(cases),'cases':cases}
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='cases'}))
print('Differing centre / edge pixels:',sum(c['centre_differences'] for c in cases),sum(c['side_differences'] for c in cases))
sys.exit(0 if result['passed'] else 1)
