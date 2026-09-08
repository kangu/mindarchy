#!/usr/bin/env python3
"""Build, deploy and verify a macOS application installer. Python 3.9+."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / 'packaging/macos'))
from verify_bundle import verify_bundle


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True, help='Numeric major.minor.patch release version')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--unsigned', action='store_true', help='Produce a local test installer, without Developer ID')
    mode.add_argument('--sign', action='store_true', help='Sign using MACOS_APP_SIGN_IDENTITY and MACOS_INSTALLER_SIGN_IDENTITY')
    parser.add_argument('--notarize', action='store_true', help='Submit to Apple using MACOS_NOTARY_PROFILE in Keychain')
    parser.add_argument('--qt', type=Path, default=Path(os.environ.get('QT_PREFIX_PATH', str(Path.home() / 'Qt/6.11.2/macos'))))
    parser.add_argument('--arch', choices=['arm64', 'x86_64', 'universal'], default=platform.machine())
    parser.add_argument('--min-macos', default='13.0')
    parser.add_argument('--output', type=Path, default=PROJECT / 'dist/macos')
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('Run this workflow on macOS.')
    if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        parser.error('--version must be major.minor.patch, e.g. 0.1.0')
    if not re.fullmatch(r'\d+\.\d+(?:\.\d+)?', args.min_macos):
        parser.error('--min-macos must be a numeric macOS version')
    args.qt = args.qt.expanduser().resolve()
    deployqt = args.qt / 'bin/macdeployqt'
    if not deployqt.is_file():
        parser.error(f'Qt deployment tool not found: {deployqt}')
    for command in ['cmake', 'ctest', 'pkgbuild', 'pkgutil', 'codesign', 'xcrun']:
        if not shutil.which(command):
            parser.error(f'Missing tool: {command}')
    application_identity = os.environ.get('MACOS_APP_SIGN_IDENTITY', '')
    installer_identity = os.environ.get('MACOS_INSTALLER_SIGN_IDENTITY', '')
    notary_profile = os.environ.get('MACOS_NOTARY_PROFILE', '')
    if args.sign and not (application_identity.startswith('Developer ID Application:') and installer_identity.startswith('Developer ID Installer:')):
        parser.error('Signed mode requires both Developer ID identities in the documented environment variables.')
    if args.notarize and not (args.sign and notary_profile):
        parser.error('--notarize requires --sign and MACOS_NOTARY_PROFILE.')
    architectures = ['arm64', 'x86_64'] if args.arch == 'universal' else [args.arch]
    # Validate the Qt kit before spending time compiling a requested architecture.
    qt_archs = subprocess.check_output(['/usr/bin/lipo', '-archs', str(args.qt / 'lib/QtCore.framework/Versions/A/QtCore')], text=True).split()
    if not set(architectures) <= set(qt_archs):
        parser.error(f'The Qt kit contains {qt_archs}, not all requested architectures: {architectures}')
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    release = args.output.expanduser().resolve() / args.version / args.arch / stamp
    release.mkdir(parents=True)
    build = PROJECT / 'build-release-macos' / args.arch
    logfile = (release / 'release.log').open('w')

    def run(command, *, env=None, capture=False, timeout=None):
        command = [str(value) for value in command]
        print('→', command[0], ' '.join(command[1:]), flush=True)
        logfile.write('\n' + repr(command) + '\n'); logfile.flush()
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE if capture else logfile,
                                stderr=logfile, timeout=timeout)
        if result.returncode:
            raise RuntimeError(f'{command[0]} exited {result.returncode}; see {release / "release.log"}')
        if capture:
            logfile.write(result.stdout); logfile.flush()
            return result.stdout

    try:
        run(['cmake', '-S', PROJECT, '-B', build, '-UQt6*_DIR', '-DCMAKE_BUILD_TYPE=Release',
             f'-DCMAKE_PREFIX_PATH={args.qt}', f'-DMINDMAP_VERSION={args.version}',
             f'-DCMAKE_OSX_ARCHITECTURES={";".join(architectures)}', f'-DCMAKE_OSX_DEPLOYMENT_TARGET={args.min_macos}'])
        run(['cmake', '--build', build, '--parallel', '4'])
        run(['ctest', '--test-dir', build, '--output-on-failure'])
        with tempfile.TemporaryDirectory(prefix='.payload-', dir=release) as temporary:
            work = Path(temporary)
            root = work / 'root'
            bundle = root / 'Applications/Mindmap Lab.app'
            bundle.parent.mkdir(parents=True)
            run(['/usr/bin/ditto', build / 'mindmap-lab.app', bundle])
            run(['/usr/bin/xattr', '-cr', bundle])
            deploy = [deployqt, bundle, f'-qmldir={PROJECT / "qml"}', '-always-overwrite', '-appstore-compliant', '-verbose=2']
            if args.sign:
                shutil.copy2(PROJECT / 'packaging/macos/MindmapLab.entitlements', bundle / 'Contents/Resources/MindmapLab.entitlements')
                deploy.append(f'-sign-for-notarization={application_identity}')
            run(deploy)
            # Qt deploys optional SQL drivers even though only its SQLite backend
            # can be needed by QML LocalStorage. Other drivers require client SDKs.
            drivers = bundle / 'Contents/PlugIns/sqldrivers'
            if drivers.exists():
                for driver in drivers.iterdir():
                    if driver.name != 'libqsqlite.dylib':
                        if driver.is_dir():
                            shutil.rmtree(driver)
                        else:
                            driver.unlink()
            # Removing optional plugins changes the outer resource seal. Nested
            # signatures from macdeployqt remain valid; reseal only the app.
            sign = ['codesign', '--force', '--sign', application_identity if args.sign else '-']
            if args.sign:
                sign += ['--options', 'runtime', '--timestamp', '--entitlements',
                         bundle / 'Contents/Resources/MindmapLab.entitlements']
            run(sign + [bundle])
            verification = verify_bundle(bundle, args.version, architectures)
            component_file = work / 'components.plist'
            run(['pkgbuild', '--analyze', '--root', root, component_file])
            with component_file.open('rb') as stream:
                components = plistlib.load(stream)
            assert len(components) == 1 and components[0]['RootRelativeBundlePath'] == 'Applications/Mindmap Lab.app'
            components[0].update(BundleIsRelocatable=False, BundleHasStrictIdentifier=True,
                                 BundleIsVersionChecked=True, BundleOverwriteAction='upgrade')
            with component_file.open('wb') as stream:
                plistlib.dump(components, stream)
            name = f'Mindmap-Lab-{args.version}-macos-{args.arch}' + ('-unsigned' if args.unsigned else '')
            package = release / f'{name}.pkg'
            command = ['pkgbuild', '--root', root, '--component-plist', component_file,
                       '--identifier', 'blue.mindmap.lab.installer', '--version', args.version,
                       '--install-location', '/', '--ownership', 'recommended']
            if args.sign:
                command += ['--sign', installer_identity]
            run(command + [package])
            if args.sign:
                run(['pkgutil', '--check-signature', package])
            if args.notarize:
                response = run(['xcrun', 'notarytool', 'submit', package, '--keychain-profile', notary_profile,
                                '--wait', '--output-format', 'json'], capture=True)
                (release / 'notarization.json').write_text(response)
                report = json.loads(response)
                if report.get('status') != 'Accepted':
                    raise RuntimeError(f'Notarization was not accepted: {report.get("status")}; submission {report.get("id")}')
                run(['xcrun', 'stapler', 'staple', package])
                run(['xcrun', 'stapler', 'validate', package])
                run(['/usr/sbin/spctl', '--assess', '--type', 'install', '--verbose=2', package])
            # Verify the actual installer payload, not just the staging copy.
            expanded = work / 'expanded'
            run(['pkgutil', '--expand-full', package, expanded])
            payloads = list(expanded.rglob('Payload/Applications/Mindmap Lab.app'))
            assert len(payloads) == 1, 'Installer must contain exactly one application'
            payload = payloads[0]
            verification = verify_bundle(payload, args.version, architectures)
            clean = {key: value for key, value in os.environ.items()
                     if not key.startswith(('QT_', 'QML', 'DYLD_'))}
            clean.update(PATH='/usr/bin:/bin:/usr/sbin:/sbin', QT_QPA_PLATFORM='cocoa')
            executable = payload / 'Contents/MacOS/mindmap-lab'
            found_version = run([executable, '--version'], env=clean, capture=True, timeout=30)
            assert args.version in found_version, 'Payload executable version mismatch'
            screenshot = release / 'payload-smoke.png'
            run([executable, '--screenshot', screenshot, '--quit-after', '1800'], env=clean, timeout=30)
            assert screenshot.is_file(), 'Packaged QML interface did not produce a screenshot'
            # Retain a convenient verified, portable app next to the installer.
            run(['/usr/bin/ditto', payload, release / 'Mindmap Lab.app'])
        digest = hashlib.sha256(package.read_bytes()).hexdigest()
        (release / 'SHA256SUMS').write_text(f'{digest}  {package.name}\n')
        manifest = {**verification, 'package': package.name, 'sha256': digest,
                    'signed': args.sign, 'notarized': args.notarize, 'minimum_macos': args.min_macos,
                    'built_at': stamp, 'qt_version': subprocess.check_output([str(args.qt / 'bin/qmake'), '-query', 'QT_VERSION'], text=True).strip(),
                    'install_path': '/Applications/Mindmap Lab.app'}
        (release / 'release.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(f'\nVerified installer: {package}\nManifest and logs: {release}', flush=True)
    finally:
        logfile.close()


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, AssertionError, subprocess.SubprocessError, OSError) as error:
        print(f'Release failed: {error}', file=sys.stderr)
        sys.exit(1)
