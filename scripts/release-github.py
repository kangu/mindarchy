#!/usr/bin/env python3
"""Upload Mindarchy installers to a verified draft GitHub release (Python 3.9+)."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ('macos', 'windows', 'omarchy')


class ReleaseError(Exception):
    pass


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def collect(version, platforms, directories):
    assets = []

    def add(path, platform, arch, expected=None, **metadata):
        if not path.is_file() or path.stat().st_size == 0:
            raise ReleaseError(f'Missing or empty artifact: {path}')
        digest = sha256(path)
        if expected is not None and digest != expected:
            raise ReleaseError(f'Checksum mismatch: {path}')
        assets.append(dict(path=path.resolve(), platform=platform, architecture=arch,
                           sha256=digest, size=path.stat().st_size, **metadata))

    for platform in platforms:
        directory = directories[platform]
        before = len(assets)
        if platform == 'macos':
            # A manifest is written only after the packaging checks succeed.
            builds = {}
            for manifest in sorted(directory.glob(f'{version}/*/*/release.json')):
                builds[manifest.parent.parent.name] = manifest
            for arch, manifest in sorted(builds.items()):
                data = json.loads(manifest.read_text())
                expected_arch = ['arm64', 'x86_64'] if arch == 'universal' else [arch]
                if (arch not in ('arm64', 'x86_64', 'universal') or data.get('version') != version
                        or sorted(data.get('architectures', [])) != sorted(expected_arch)):
                    raise ReleaseError(f'Invalid version/architecture in {manifest}')
                if type(data.get('signed')) is not bool or type(data.get('notarized')) is not bool:
                    raise ReleaseError(f'Missing signing metadata: {manifest}')
                if data['notarized'] and not data['signed']:
                    raise ReleaseError(f'Inconsistent signing metadata: {manifest}')
                suffix = '' if data['signed'] else '-unsigned'
                for key, ext, hashkey in [('package', 'pkg', 'sha256'), ('dmg', 'dmg', 'dmg_sha256')]:
                    name = f'Mindarchy-{version}-macos-{arch}{suffix}.{ext}'
                    if data.get(key) != name or not re.fullmatch(r'[0-9a-f]{64}', data.get(hashkey, '')):
                        raise ReleaseError(f'Invalid artifact name/checksum in {manifest}')
                    add(manifest.parent / name, platform, arch, data[hashkey],
                        signed=data['signed'], notarized=data['notarized'])
        elif platform == 'windows':
            for path in sorted(directory.glob(f'Mindarchy-{version}-windows-x64-setup.exe')):
                add(path, platform, 'x64')
        else:
            seen = set()
            pattern = re.compile(r'mindarchy-' + re.escape(version) + r'-\d+(?:\.\d+)?-(x86_64|aarch64)\.pkg\.tar\.(zst|xz|gz)')
            for path in sorted(directory.glob('mindarchy-*.pkg.tar.*')):
                match = pattern.fullmatch(path.name)
                if not match:
                    continue
                arch = match[1]
                if arch in seen:
                    raise ReleaseError(f'Multiple Omarchy packages for {arch}; stage one package revision per architecture.')
                seen.add(arch)
                add(path, platform, arch)
                signature = path.with_name(path.name + '.sig')
                if signature.exists():
                    add(signature, platform, arch)
        if len(assets) == before:
            raise ReleaseError(f'No {platform} installers for {version} in {directory}. '
                               'Copy the completed build here or choose an explicit --platforms subset.')
    return sorted(assets, key=lambda item: item['path'].name)


def gh(*args):
    result = subprocess.run(['gh', *args], capture_output=True, text=True)
    if result.returncode:
        raise ReleaseError(result.stderr.strip() or result.stdout.strip() or 'GitHub CLI failed')
    return result.stdout


def publish(repo, tag, files, notes, make_public, prerelease, resume):
    # Listing avoids interpreting authentication/network errors as "release absent".
    releases = gh('api', '--hostname', 'github.com', f'repos/{repo}/releases?per_page=100',
                  '--paginate', '--jq', '.[] | {id, tag_name, draft}')
    existing = next((json.loads(line) for line in releases.splitlines()
                     if line.strip() and json.loads(line).get('tag_name') == tag), None)
    remote_repo = f'github.com/{repo}'
    if existing:
        if not existing['draft']:
            raise ReleaseError('This release is already public; use a new version. No assets were changed.')
        if not resume:
            raise ReleaseError('A draft already exists. Use --resume to verify it and upload missing assets.')
    elif resume:
        raise ReleaseError('No draft exists to resume. Run without --resume to create it.')
    else:
        args = ['release', 'create', tag, '--repo', remote_repo, '--draft', '--verify-tag',
                '--title', f'Mindarchy {tag}', '--notes-file', str(notes)]
        if prerelease:
            args.append('--prerelease')
        gh(*args)

    # Download existing draft assets before a resume; never overwrite a different binary.
    with tempfile.TemporaryDirectory(prefix='mindarchy-verify-') as folder:
        downloaded = Path(folder)
        if existing:
            asset_json = gh('api', '--hostname', 'github.com',
                            f"repos/{repo}/releases/{existing['id']}/assets?per_page=100",
                            '--paginate', '--jq', '.[] | {name}')
            names = [json.loads(line)['name'] for line in asset_json.splitlines() if line.strip()]
            expected = {path.name for path in files}
            if len(names) != len(set(names)) or set(names) - expected:
                raise ReleaseError('Draft has duplicate or unexpected assets; inspect the draft before resuming.')
            if names:
                gh('release', 'download', tag, '--repo', remote_repo, '--dir', str(downloaded))
            for path in files:
                remote = downloaded / path.name
                if remote.exists() and sha256(remote) != sha256(path):
                    raise ReleaseError(f'Existing draft asset differs: {path.name}. No replacement was uploaded.')
        for path in files:
            if not (downloaded / path.name).exists():
                print(f'Uploading {path.name}', flush=True)
                gh('release', 'upload', tag, str(path), '--repo', remote_repo)
        # Round-trip verification also works with older assets lacking API digests.
        gh('release', 'download', tag, '--repo', remote_repo, '--dir', str(downloaded), '--clobber')
        if {p.name for p in downloaded.iterdir()} != {p.name for p in files}:
            raise ReleaseError('Uploaded asset set differs from the planned release. Draft retained.')
        for path in files:
            if sha256(downloaded / path.name) != sha256(path):
                raise ReleaseError(f'Uploaded checksum mismatch: {path.name}. Draft retained.')
    args = ['release', 'edit', tag, '--repo', remote_repo, '--notes-file', str(notes),
            '--title', f'Mindarchy {tag}', f'--prerelease={str(prerelease).lower()}']
    if make_public:
        args.append('--draft=false')
    gh(*args)
    print(f'{"Published" if make_public else "Draft ready"}: https://github.com/{repo}/releases/tag/{tag}')


def release_notes(version, repo, assets, extra=''):
    lines = [f'# Mindarchy {version}', '', '[Website](https://mindarchy.xyz)', '']
    if extra.strip():
        lines.extend([extra.strip(), ''])
    lines.extend(['## Downloads', '', '| Platform | Architecture | Download |', '| --- | --- | --- |'])
    for asset in assets:
        name = asset['path'].name
        if not name.endswith('.sig'):
            lines.append(f"| {asset['platform']} | {asset['architecture']} | [{name}](https://github.com/{repo}/releases/download/v{version}/{name}) |")
    lines.extend(['', '## Installation', ''])
    platforms = {a['platform'] for a in assets}
    if 'omarchy' in platforms:
        lines.append('- Omarchy: install the matching package with `sudo pacman -U <downloaded-package.pkg.tar.zst>`.')
    if 'macos' in platforms:
        lines.append('- macOS: use either the PKG installer or the DMG; you do not need both.')
        for arch in sorted({a['architecture'] for a in assets if a['platform'] == 'macos'}):
            item = next(a for a in assets if a['platform'] == 'macos' and a['architecture'] == arch)
            lines.append(f"- macOS {arch}: {'signed' if item['signed'] else 'unsigned'}, "
                         f"{'notarized' if item['notarized'] else 'not notarized'} (build manifest).")
        if any(a.get('signed') is False or a.get('notarized') is False for a in assets if a['platform'] == 'macos'):
            lines.append('- macOS may require approval in System Settings → Privacy & Security for builds that are not signed and notarized.')
    if 'windows' in platforms:
        lines.append('- Windows: run the x64 setup EXE. Signing status is not recorded by the current Windows build script.')
    lines.extend(['', '## Checksums', '', 'SHA256SUMS covers every installer and optional package signature. '
                  'Verify with `sha256sum -c SHA256SUMS` on Linux or `shasum -a 256 -c SHA256SUMS` on macOS '
                  '(download the listed assets first). On Windows, compare `Get-FileHash <installer> -Algorithm SHA256` '
                  'with the corresponding entry.', ''])
    return '\n'.join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True, help='Exact package version, e.g. 0.1.4; uses tag v0.1.4')
    parser.add_argument('--repo', default='kangu/mindarchy', help='GitHub OWNER/REPO')
    parser.add_argument('--platforms', default=','.join(PLATFORMS), help='Comma-separated required platforms: macos,windows,omarchy')
    for platform, default in [('macos', ROOT / 'dist/macos'), ('windows', ROOT / 'artifacts/windows'), ('omarchy', ROOT / 'artifacts/omarchy')]:
        parser.add_argument(f'--{platform}-dir', type=Path, default=default)
    parser.add_argument('--notes-file', type=Path, help='Markdown changes to prepend to generated download/installation notes')
    parser.add_argument('--dry-run', action='store_true', help='Validate and show the release without GitHub access')
    parser.add_argument('--prerelease', action='store_true', help='Mark as a preview/pre-release')
    parser.add_argument('--publish', action='store_true', help='Publish after all uploaded files pass verification; default is draft')
    parser.add_argument('--resume', action='store_true', help='Resume an existing draft; never replace differing assets')
    args = parser.parse_args(argv)
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        parser.error('--version must be major.minor.patch')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9_.-]+', args.repo):
        parser.error('--repo must be OWNER/REPO on github.com')
    platforms = list(dict.fromkeys(args.platforms.split(',')))
    if not platforms or any(p not in PLATFORMS for p in platforms):
        parser.error('--platforms must contain macos, windows and/or omarchy, separated by commas')
    try:
        assets = collect(args.version, platforms, {p: getattr(args, f'{p}_dir') for p in PLATFORMS})
        notes = release_notes(args.version, args.repo, assets,
                              args.notes_file.read_text(encoding='utf-8') if args.notes_file else '')
        print(f'Repository: {args.repo}\nTag: v{args.version}\nMode: {"publish" if args.publish else "draft"}')
        for asset in assets:
            print(f"{asset['sha256']}  {asset['path']} ({asset['size']} bytes)")
        if args.dry_run:
            print('\nAlso uploads: SHA256SUMS, release-assets.json\n\n' + notes)
            print('Dry run complete. GitHub authentication and existing remote tag will be checked on upload.')
            return 0
        if not shutil.which('gh'):
            raise ReleaseError('Install GitHub CLI (https://cli.github.com), then run gh auth login.')
        with tempfile.TemporaryDirectory(prefix='mindarchy-release-') as folder:
            stage = Path(folder)
            # Freeze exactly the bytes validated above before any network mutation.
            files = []
            for asset in assets:
                dest = stage / asset['path'].name
                shutil.copyfile(asset['path'], dest)
                if sha256(dest) != asset['sha256']:
                    raise ReleaseError(f'Artifact changed during staging: {dest.name}')
                files.append(dest)
            checksums = stage / 'SHA256SUMS'
            checksums.write_text(''.join(f"{a['sha256']}  {a['path'].name}\n" for a in assets), encoding='utf-8')
            manifest = stage / 'release-assets.json'
            manifest.write_text(json.dumps({'version': args.version, 'assets': [
                dict({k: v for k, v in a.items() if k != 'path'}, name=a['path'].name) for a in assets
            ]}, indent=2) + '\n', encoding='utf-8')
            notes_path = stage / 'notes.md'
            notes_path.write_text(notes, encoding='utf-8')
            publish(args.repo, f'v{args.version}', files + [checksums, manifest], notes_path,
                    args.publish, args.prerelease, args.resume)
        return 0
    except (ReleaseError, OSError, ValueError, KeyError) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
