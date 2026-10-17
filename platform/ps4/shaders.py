import pathlib, struct, subprocess, sys
compiler, output = sys.argv[1:]
root = pathlib.Path(__file__).resolve().parent
output = pathlib.Path(output)
parts = ['#pragma once', '#include <cstdint>']
for stage in ('vert', 'frag'):
    binary = output / ('frame.' + stage + '.spv')
    subprocess.run([compiler, '-V', '--target-env', 'vulkan1.0', str(root / ('frame.' + stage)), '-o', str(binary)], check=True)
    data = binary.read_bytes()
    words = struct.unpack('<' + 'I' * (len(data) // 4), data)
    parts.append('static const uint32_t frame_' + stage + '[] = {' + ','.join(hex(w) for w in words) + '};')
(output / 'shaders.h').write_text('\n'.join(parts) + '\n')
