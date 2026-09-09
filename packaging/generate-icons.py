#!/usr/bin/env python3
"""Generate native icon containers from the supplied artwork on macOS (sips/iconutil)."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = root / 'assets/mindarchy-icon.png'
out = root / 'assets/icons'
out.mkdir(parents=True, exist_ok=True)
for size in (16, 24, 32, 48, 64, 128, 256, 512, 1024):
    subprocess.run(['sips', '-z', str(size), str(size), str(source), '--out',
                    str(out / f'mindarchy-{size}.png')], check=True, stdout=subprocess.DEVNULL)
with tempfile.TemporaryDirectory() as tmp:
    iconset = Path(tmp) / 'mindarchy.iconset'
    iconset.mkdir()
    subprocess.run(['swift', str(root / 'packaging/macos/generate-icon.swift'),
                    str(source), tmp], check=True)
    shutil.copyfile(Path(tmp) / 'mac-512.png', out / 'mindarchy-macos-512.png')
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            suffix = '@2x' if scale == 2 else ''
            shutil.copyfile(Path(tmp) / f'mac-{size * scale}.png',
                            iconset / f'icon_{size}x{size}{suffix}.png')
    subprocess.run(['iconutil', '-c', 'icns', str(iconset), '-o',
                    str(out / 'mindarchy.icns')], check=True)
# Modern Windows accepts PNG-compressed images in multi-resolution ICO files.
sizes = (16, 24, 32, 48, 64, 128, 256)
images = [(out / f'mindarchy-{size}.png').read_bytes() for size in sizes]
offset = 6 + 16 * len(images)
entries = []
for size, data in zip(sizes, images):
    entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
    offset += len(data)
(out / 'mindarchy.ico').write_bytes(struct.pack('<HHH', 0, 1, len(images)) + b''.join(entries) + b''.join(images))
shutil.copyfile(out / 'mindarchy-512.png', out / 'blue.mindmap.lab.png')
print('Created PNG sizes, macOS ICNS and Windows ICO in', out)
