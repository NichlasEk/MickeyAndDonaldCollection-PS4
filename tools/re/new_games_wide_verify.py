#!/usr/bin/env python3
"""ROM-private widescreen regression. Build MDC_HEADLESS, then run this file.
Outputs only to build/widescreen by default; never publish snapshots or captures.
The frozen-pixel oracle fills a separate vanilla VDP's nametable from the decoded
map and checks its extension pixels against the module's rendering hook.
"""
import argparse, ctypes as C, json, os, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from mdt import MD, ModuleMD, _dll
_dll.mdt_render_only.argtypes=[C.c_void_p]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--roms',type=Path,default=ROOT/'roms')
parser.add_argument('--out',type=Path,default=ROOT/'build/widescreen')
parser.add_argument('--frames',type=int,default=240)
parser.add_argument('--games',default='lion,aladdin')
parser.add_argument('--unpatched-baseline',action='store_true',help='also changes sprite ROM patches; may change game timing, not just rendering')
args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
SPECS={'lion':('Lion King.bin',10),'aladdin':('Aladdin_USA_Final_Cut.md',11)}

def oracle(path,game,state,info):
    candidate=ModuleMD(str(path),game);candidate.load(state);candidate.configure(40)
    ref=MD(str(path));ref.load(state);ref.set_ext(40,False)
    ram=bytes(ref.ram);rom=bytes(ref.rom)
    def r16(addr):
        addr&=0xffffff;data=ram if addr>=0xff0000 else rom
        off=(addr&65535) if addr>=0xff0000 else addr
        if off+1>=len(data):raise RuntimeError('oracle read out of bounds')
        return int.from_bytes(data[off:off+2],'big')
    def r32(addr):return r16(addr)*65536+r16(addr+2)
    if game=='lion':
        cam=r16(0xcd36|0xff0000);cx=r16(0xff0000+cam+8);cy=r16(0xff0000+cam+10)
        cx=(cx+32768)%65536-32768;cy=(cy+32768)%65536-32768
        stride=r16(0xffcd1e);height=r16(0xffcd20)*16;width=stride*8
        defs=r32(0xffcd2a)&0xffffff;mapbase=r32(0xffcd22)&0xffffff;bank=0
        def cell(tx,ty):return defs+(r16(mapbase+ty//2*stride+tx//2*2)&0xfffe)+(ty&1)*4+(tx&1)*2
    else:
        cx=r16(0xff7e06)+16;cy=r16(0xff7e08)+16;width=r16(0xff7dc8);height=r16(0xff7dcc)
        defs=r32(0xff7dce)&0xffffff;bank=bool(ram[0xf695])
        def cell(tx,ty):return defs+r16(r32(0xff9896+ty//2*64)+tx//2*2)+(ty&1)*4+(tx&1)*2
    # Rug Ride uses an already-filled repeating plane, not the collision map.
    repeat=(game=='aladdin' and ram[0x7e36]==8)
    if not repeat:
        # Anchor the map to the scroll registers latched for the frozen image.
        hsbase=(ref.regs[13]&63)<<10
        hs=int.from_bytes(bytes(ref.vram[hsbase+bank*2:hsbase+bank*2+2]),'big')
        cx+=(-hs-cx+256)%512-256
        cy+=(int(ref.vsram[bank])-cy+128)%256-128
        base=(ref.regs[4]&7)<<13 if bank else (ref.regs[2]&0x38)<<10
        for ty in range(cy//8,(cy+223)//8+1):
            for tx in range((cx-40)//8,(cx+359)//8+1):
                if not(0<=tx*8<width and 0<=ty*8<height):continue
                value=r16(cell(tx,ty));dest=base+(ty&31)*128+(tx&63)*2
                ref.vram[dest]=value>>8;ref.vram[dest+1]=value&255
    for obj in (candidate,ref):obj.debug_layers(2 if bank else 1);_dll.mdt_render_only(obj.h)
    w,h,p=candidate.frame_raw();rw,rh,q=ref.frame_raw();assert (w,h)==(rw,rh)
    checked=bad=0
    for y in range(h):
        for x in list(range(info['mask_left'],40))+list(range(w-40,w-info['mask_right'])):
            if not repeat and not ((16 if game=='aladdin' else 0)<=cx+x-40<width and 0<=cy+y<height):continue
            checked+=1;bad+=p[y*w+x]!=q[y*w+x]
    return checked,bad

results=[];failed=False
for game in args.games.split(','):
    name,count=SPECS[game];path=args.roms/name
    for entry in range(count):
        wide=ModuleMD(str(path),game);wide.configure(40)
        if not wide.start_level(entry):raise RuntimeError(f'{game} entry {entry} did not boot')
        start=wide.save();narrow=ModuleMD(str(path),game);narrow.load(start)
        # Isolate rendering width from ROM sprite submission and sprite limits.
        # The optional baseline separately measures changes to game execution.
        narrow.configure(0 if args.unpatched_baseline else 40)
        if not args.unpatched_baseline:narrow.set_ext(0,False)
        snapshots=[];center_diff=0
        for frame in range(args.frames):
            direction=('LEFT' if game=='lion' and entry in (6,9) else 'RIGHT') if frame<args.frames//2 else 'LEFT'
            pad=direction+('+C' if frame//30%2 else '')
            wide.hold(pad);narrow.hold(pad);info=wide.mrun(1);narrow.mrun(1)
            if frame in (0,args.frames//2,args.frames-1):
                nw,nh,np=narrow.frame_raw();ww,wh,wp=wide.frame_raw()
                diff=sum(np[y*nw+x]!=wp[y*ww+x+(ww-nw)//2] for y in range(nh) for x in range(nw));center_diff+=diff
                checked,bad=oracle(path,game,wide.save(),info) if info['gameplay'] and not(game=='lion' and entry==3) else (0,0)
                snapshots.append({'frame':frame,'center_changed_pixels':diff,'extension_pixels_checked':checked,'extension_pixels_wrong':bad,**info})
        saved=wide.save();wide.hold(0);wide.mrun(30);expected=wide.save();wide.load(saved);wide.mrun(30)
        replay=wide.save()==expected
        wide.png(str(args.out/f'{game}-{entry:02d}.png'))
        (args.out/f'{game}-{entry:02d}.state').write_bytes(saved)
        ok=not center_diff and replay and all(r['extension_pixels_wrong']==0 for r in snapshots)
        row={'game':game,'entry':entry,'passed':ok,'state_replay':replay,'snapshots':snapshots,
             'note':'Stampede scenery reconstruction: see lion_stampede_verify for dedicated checks' if game=='lion' and entry==3 else ''}
        results.append(row);failed|=not ok;print(json.dumps(row),flush=True)
        (args.out/'results.json').write_text(json.dumps({'host_only':True,'same_sprite_patches':not args.unpatched_baseline,'frames_per_entry':args.frames,'cases':results},indent=2)+'\n')
raise SystemExit(1 if failed else 0)
