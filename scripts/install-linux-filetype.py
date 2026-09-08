#!/usr/bin/env python3
"""Install Mindmap Lab and its .omm association/thumbnailer for the current user."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=project / 'build/mindmap-lab')
parser.add_argument('--system', action='store_true', help='Install under /usr/local for sandboxed Nautilus thumbnails (requires root)')
args = parser.parse_args()
if args.system and os.geteuid() != 0:
    parser.error('--system requires root')
binary = args.binary.resolve()
if not binary.is_file():
    parser.error(f'Build the app first: {binary}')
data = Path('/usr/local/share') if args.system else Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share')))
bin_dir = Path('/usr/local/bin') if args.system else Path.home() / '.local/bin'
bin_dir.mkdir(parents=True, exist_ok=True)
installed = bin_dir / 'mindmap-lab'
# Atomic replacement also works while an older executable is running.
with tempfile.NamedTemporaryFile(dir=bin_dir, delete=False) as stream:
    temporary = Path(stream.name)
try:
    shutil.copy2(binary, temporary)
    temporary.chmod(0o755)
    temporary.replace(installed)
finally:
    temporary.unlink(missing_ok=True)

def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(project / source, destination)

copy('packaging/blue.mindmap.omm.xml', data / 'mime/packages/blue.mindmap.omm.xml')
copy('assets/icons/mindmap-blue-512.png', data / 'icons/hicolor/512x512/apps/blue.mindmap.lab.png')
# Desktop Exec values have their own escaping rules (not shell syntax).
quoted = '"' + str(installed).replace('\\','\\\\').replace('"','\\"').replace('`','\\`').replace('$','\\$') + '"'
for source, destination in [('packaging/blue.mindmap.lab.desktop', 'applications/blue.mindmap.lab.desktop'),
                            ('packaging/blue.mindmap.lab.thumbnailer', 'thumbnailers/blue.mindmap.lab.thumbnailer')]:
    text = (project / source).read_text().replace('Exec=mindmap-lab', 'Exec=' + quoted)
    # TryExec is a path, not a command line.
    text = text.replace('TryExec=' + quoted, 'TryExec=' + str(installed))
    target = data / destination
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text)
subprocess.run(['update-mime-database', str(data / 'mime')], check=True)
subprocess.run(['update-desktop-database', str(data / 'applications')], check=True)
if not args.system:
    subprocess.run(['xdg-mime', 'default', 'blue.mindmap.lab.desktop', 'application/x-omm+json'], check=True)
print(f'Installed {installed}; OMM metadata registered. JSON file associations are unchanged.')
if not args.system:
    print('Nautilus sandboxed thumbnails need the --system installation; file opening and standalone PNG rendering work with this user installation.')
