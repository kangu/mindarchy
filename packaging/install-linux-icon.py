#!/usr/bin/env python3
"""Install this development build's launcher and icons for the current Linux user."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
executable = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'build/mindarchy'
if not executable.is_file() or not os.access(executable, os.X_OK):
    raise SystemExit(f'Build the application first: {executable}')
data = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share')))
for size in (16, 24, 32, 48, 64, 128, 256, 512, 1024):
    target = data / f'icons/hicolor/{size}x{size}/apps/org.mindarchy.app.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / f'assets/icons/mindarchy-{size}.png', target)
entry = data / 'applications/org.mindarchy.app.desktop'
entry.parent.mkdir(parents=True, exist_ok=True)
quoted = str(executable)
for char in ('\\', '"', '`', '$'):
    quoted = quoted.replace(char, '\\' + char)
text = (root / 'packaging/org.mindarchy.app.desktop').read_text().replace('Exec=mindarchy', f'Exec="{quoted}"')
entry.write_text(text)
for command, args in [('update-desktop-database', [str(entry.parent)]),
                      ('gtk-update-icon-cache', ['-f', '-t', str(data / 'icons/hicolor')])]:
    if shutil.which(command):
        subprocess.run([command, *args], check=False)
print('Installed', entry)
