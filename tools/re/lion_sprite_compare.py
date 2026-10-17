#!/usr/bin/env python3
"""Compare Lion's host sprites against a historical mdtool with ROM clip patches.
Use a library built from commit 67a4e5f as --reference-lib. Both builds run your
local ROM; all images/states/raw pixels stay in the ignored output directory.
"""
import argparse
import array
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference-lib',type=Path,required=True)
p.add_argument('--candidate-lib',type=Path,default=ROOT/'build/host/libmdtool.so')
p.add_argument('--rom',type=Path,default=ROOT/'roms/Lion King.bin')
p.add_argument('--out',type=Path,default=ROOT/'build/lion-sprite-compare')
p.add_argument('--phase',choices=('reference','candidate'),help=argparse.SUPPRESS)
a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
if not a.phase:
    for library in (a.reference_lib,a.candidate_lib):
        if not library.is_file():p.error(f'missing library: {library}')
    if hashlib.sha256(a.reference_lib.read_bytes()).digest()==hashlib.sha256(a.candidate_lib.read_bytes()).digest():
        p.error('reference and candidate must be different builds')
    for phase,library in (('reference',a.reference_lib),('candidate',a.candidate_lib)):
        env=dict(os.environ,MDTOOL_DLL=str(library.resolve()))
        subprocess.run([sys.executable,__file__,*sys.argv[1:],'--phase',phase],env=env,check=True)
    result=json.loads((a.out/'results.json').read_text())
    result['reference_sha256']=hashlib.sha256(a.reference_lib.read_bytes()).hexdigest()
    result['candidate_sha256']=hashlib.sha256(a.candidate_lib.read_bytes()).hexdigest()
    (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(a.out/'results.json')
    raise SystemExit(0)

sys.path.insert(0,str(ROOT/'tools'))
from mdt import MD,ModuleMD,_dll
_dll.mdt_render_only.argtypes=[C.c_void_p]
rows=[]
for level in (0,1,2,4,5,6,7,8,9):
    m=ModuleMD(str(a.rom),'lion');m.configure(40)
    state_path=a.out/f'{level}-boot.state'
    if a.phase=='reference':
        if not m.start_level(level):raise RuntimeError('level boot failed')
        state=m.save()
        if int.from_bytes(state[4:8],sys.byteorder)!=2:
            raise RuntimeError('Reference must be the pre-host-sprite v2 build (67a4e5f)')
        state_path.write_bytes(state)
    else:
        m.load(state_path.read_bytes());m.configure(40)
    m.debug_layers(4)
    for frame in range(180):
        m.hold(('LEFT' if level in (6,9) else 'RIGHT') if frame<120 else 'LEFT')
        m.mrun(1)
        if frame not in (2,30,90,179):continue
        width,height,pixels=m.frame_raw();raw_path=a.out/f'{level}-{frame}.raw'
        if a.phase=='reference':
            raw_path.write_bytes(array.array('H',pixels).tobytes());continue
        reference=array.array('H');reference.frombytes(raw_path.read_bytes())
        assert len(reference)==len(pixels)
        edge=sum(pixels[y*width+x]!=reference[y*width+x] for y in range(height)
                 for x in (*range(40),*range(width-40,width)))
        center=sum(pixels[y*width+x]!=reference[y*width+x] for y in range(height) for x in range(40,width-40))
        # A vanilla clone has only the original SAT. Ensure the new side path
        # really contributes visible sprite pixels, rather than passing blank.
        state=m.save();candidate=ModuleMD(str(a.rom),'lion');candidate.load(state);candidate.configure(40)
        vanilla=MD(str(a.rom));vanilla.load(state);vanilla.set_ext(40,False)
        for obj in (candidate,vanilla):obj.debug_layers(4);_dll.mdt_render_only(obj.h)
        w,h,c=candidate.frame_raw();vw,vh,v=vanilla.frame_raw();assert (w,h)==(vw,vh)
        added=sum(c[y*w+x]!=v[y*w+x] for y in range(h) for x in (*range(40),*range(w-40,w)))
        row={'entry':level,'frame':frame,'edge_wrong':edge,'center_wrong':center,'pixels_different_from_original_sat':added}
        rows.append(row);print(json.dumps(row),flush=True)
if a.phase=='candidate':
    result={'cases':rows,'host_only':True,'reference_commit':'67a4e5f'}
    (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    if any(r['edge_wrong'] or r['center_wrong'] for r in rows):raise SystemExit(1)
    if not sum(r['pixels_different_from_original_sat'] for r in rows):raise SystemExit('No added sprite pixels exercised')
