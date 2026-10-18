#!/usr/bin/env python3
"""Build a ROM-free PS4 test package from the existing OpenOrbis EBOOT."""
import hashlib, json, os, shutil, subprocess, struct
from pathlib import Path
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parents[1]
sdk = Path(os.environ.get('OO_PS4_TOOLCHAIN', '/opt/openorbis/OpenOrbis/PS4Toolchain'))
stack = Path(os.environ.get('PS4_STACK', root.parent / 'ScummVM-PS4/.tools/opengnm-stack')).resolve()
stage = root / 'build/pkg-011'
stage.mkdir(parents=True, exist_ok=True)
(root / 'dist').mkdir(exist_ok=True)
tool = sdk / 'bin/linux/PkgTool.Core'
content = 'IV0000-MDCL00001_00-MICKEYDONALD0001'
def run(*args): subprocess.run(list(map(str, args)), check=True, cwd=stage)
def copy(source, name):
    target = stage / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    return name
files = [copy(root / 'build/runtime/eboot.bin', 'eboot.bin'), copy(root / 'LICENSE', 'licenses/MickeyDonald-MIT.txt')]
sample = sdk / 'samples/hello_world'
for name in ('sce_sys/about/right.sprx', 'sce_module/libc.prx', 'sce_module/libSceFios2.prx'):
    files.append(copy(sample / name, name))
for name in ('vulkan-ps4', 'opengnm', 'opengnm-psbc'):
    files.append(copy(stack / name / 'LICENSE', 'licenses/' + name + '.txt'))
files.append(copy(sdk / 'LICENSE', 'licenses/OpenOrbis.txt'))
# Dashboard icon: 512x512, 8-bit opaque RGB PNG, non-interlaced.
icon_path=root / 'res/ps4_icon0.png'
icon=icon_path.read_bytes()
if icon[:8]!=b'\x89PNG\r\n\x1a\n' or icon[12:16]!=b'IHDR':
    raise SystemExit('Invalid dashboard PNG')
if struct.unpack('>IIBBBBB',icon[16:29])!=(512,512,8,2,0,0,0):
    raise SystemExit('Dashboard icon must be 512x512 RGB8, non-interlaced')
files.append(copy(icon_path,'sce_sys/icon0.png'))
sfo = stage / 'sce_sys/param.sfo'
if sfo.exists(): sfo.unlink()
run(tool, 'sfo_new', sfo)
for key, kind, size, value in [
    ('APP_TYPE','Integer',4,'1'), ('APP_VER','Utf8',8,'00.11'), ('ATTRIBUTE','Integer',4,'0'),
    ('CATEGORY','Utf8',4,'gd'), ('CONTENT_ID','Utf8',48,content), ('DOWNLOAD_DATA_SIZE','Integer',4,'0'),
    ('SYSTEM_VER','Integer',4,'0'), ('TITLE','Utf8',128,'Mickey & Donald Collection'),
    ('TITLE_ID','Utf8',12,'MDCL00001'), ('VERSION','Utf8',8,'00.11')]:
    run(tool,'sfo_setentry',sfo,key,'--type',kind,'--maxsize',size,'--value',value)
files.append('sce_sys/param.sfo')
# Explicit allowlist: roms/ and build/local-roms/ are never packaged.
run(sdk / 'bin/linux/create-gp4', '-out=project.gp4', '-content-id=' + content, '-files=' + ' '.join(files))
# The SDK GP4 generator omits arbitrary directories such as licenses/.
project = stage / 'project.gp4'
tree = ET.parse(project)
directory = tree.getroot().find('rootdir')
directory.clear()
for name in files:
    node = directory
    for part in Path(name).parts[:-1]:
        child = next((x for x in node if x.get('targ_name') == part), None)
        if child is None: child = ET.SubElement(node, 'dir', targ_name=part)
        node = child
ET.indent(tree)
tree.write(project, encoding='utf-8', xml_declaration=True)
run(tool,'pkg_build','project.gp4',stage)
package = root / 'dist/MickeyDonald-PS4-0.11-Test.pkg'
shutil.copyfile(stage / (content + '.pkg'), package)
run(tool,'pkg_validate',package)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
digest = sha(package)
package.with_suffix('.pkg.sha256').write_text(digest + '  ' + package.name + '\n')
manifest = {'package':package.name, 'sha256':digest, 'contains_roms':False, 'physical_ps4_tested':False,
            'files':{name:sha(stage / name) for name in files},
            'graphics_archives':{name:sha(stack / name) for name in ('vulkan-ps4/libvulkan_ps4.a','opengnm/libopengnm.a','opengnm-psbc/libpsbc.orbis.a')}}
package.with_suffix('.manifest.json').write_text(json.dumps(manifest,indent=2) + '\n')
print(package, digest)
