#!/usr/bin/env python3
"""Check every reachable blob before publishing; never print sensitive contents."""
import re, subprocess
from pathlib import PurePosixPath
objects = subprocess.check_output(['git','rev-list','--objects','--all'],text=True).splitlines()
banned = {'.bin','.gen','.smd','.32x','.state','.pads','.pkg','.zip','.7z','.rar','.wav','.pcm','.mkv','.mp4','.npz','.npy','.jks','.keystore','.pem','.key','.pfx','.elf','.oelf','.self','.prx','.a','.o','.so'}
secret = re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{36,}|AKIA[A-Z0-9]{16}')
issues=[]; blobs=0; total=0
proc=subprocess.Popen(['git','cat-file','--batch'],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
for record in objects:
    oid, _, name = record.partition(' ')
    proc.stdin.write((oid+'\n').encode()); proc.stdin.flush()
    header=proc.stdout.readline().decode().split()
    if len(header)!=3: raise SystemExit('Unexpected git object response')
    size=int(header[2]); data=proc.stdout.read(size); proc.stdout.read(1)
    if header[1]!='blob': continue
    blobs+=1; total+=size
    path=PurePosixPath(name)
    if path.suffix.lower() in banned or any(part in ('roms','saves','screenshots','build','dist') for part in path.parts):
        issues.append((name,'game data or generated binary path'))
    if path.name=='.env' or path.name=='keystore.properties': issues.append((name,'local configuration path'))
    if data[0x100:0x110].startswith(b'SEGA') or data[0x300:0x310].startswith(b'SEGA'):
        issues.append((name,'Mega Drive ROM signature'))
    if secret.search(data): issues.append((name,'credential signature'))
proc.stdin.close(); proc.wait()
for name,reason in sorted(set(issues)): print(f'REJECT {name}: {reason}')
if issues: raise SystemExit(1)
print(f'PASS: {blobs} historical blobs, {total} bytes; no ROM signatures, banned artifacts or credential signatures')
print('Scope: all local refs; heuristic credential checks are not a universal secret detector.')
