#!/usr/bin/env python3
"""Check the deployed application's metadata, architectures and Mach-O linkage."""
import pathlib
import plistlib
import re
import subprocess


def output(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def verify_bundle(bundle, version, architectures):
    bundle = pathlib.Path(bundle).resolve()
    with (bundle / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    assert info['CFBundleIdentifier'] == 'blue.mindmap.lab', 'Unexpected bundle identity'
    assert info['CFBundleShortVersionString'] == version, 'Incorrect release version'
    executable = bundle / 'Contents/MacOS' / info['CFBundleExecutable']
    assert executable.is_file(), 'Missing application executable'
    extension = bundle / 'Contents/PlugIns/OMMPreview.appex'
    with (extension / 'Contents/Info.plist').open('rb') as stream:
        preview_info = plistlib.load(stream)
    assert preview_info['CFBundleShortVersionString'] == version, 'Preview extension version mismatch'
    attributes = preview_info['NSExtension']['NSExtensionAttributes']
    assert attributes['QLIsDataBasedPreview'] and 'blue.mindmap.omm' in attributes['QLSupportedContentTypes']
    assert (extension / 'Contents/PlugIns/platforms/libqoffscreen.dylib').is_file(), 'Preview sandbox needs its own platform plugin'
    binaries = []
    for path in bundle.rglob('*'):
        if path.is_symlink():
            assert path.resolve().is_relative_to(bundle) and path.exists(), f'External or broken bundle symlink: {path}'
        if path.is_file() and not path.is_symlink() and 'Mach-O' in output('/usr/bin/file', '-b', str(path)):
            binaries.append(path)
    assert binaries, 'No Mach-O payload'

    def entrypoint(binary):
        for parent in binary.parents:
            if parent.suffix == '.appex':
                with (parent / 'Contents/Info.plist').open('rb') as stream:
                    metadata = plistlib.load(stream)
                return parent / 'Contents/MacOS' / metadata['CFBundleExecutable']
        return executable

    def expand(path, binary):
        return pathlib.Path(path.replace('@loader_path', str(binary.parent)).replace('@executable_path', str(entrypoint(binary).parent)))

    def rpaths(binary):
        load = output('/usr/bin/otool', '-l', str(binary))
        return re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', load)

    for binary in binaries:
        actual = set(output('/usr/bin/lipo', '-archs', str(binary)).split())
        assert set(architectures) <= actual, f'Wrong architecture in {binary}: {actual}'
        owner = entrypoint(binary)
        search_paths = rpaths(binary)
        for path in search_paths:
            if path.startswith('/'):
                assert path.startswith(('/System/', '/usr/lib/')), f'External runpath: {binary}: {path}'
        linked = output('/usr/bin/otool', '-L', str(binary))
        for line in linked.splitlines():
            if not line.startswith('\t'):
                continue
            dependency = line.strip().split(' (compatibility version')[0]
            if dependency.startswith(('/System/', '/usr/lib/')):
                continue
            assert dependency.startswith('@'), f'External dependency: {binary}: {dependency}'
            if dependency.startswith('@rpath/'):
                suffix = dependency[len('@rpath/'):]
                candidates = [expand(path, binary) / suffix for path in search_paths]
                candidates += [expand(path, owner) / suffix for path in rpaths(owner)]
            else:
                candidates = [expand(dependency, binary)]
            assert any(p.exists() and p.resolve().is_relative_to(bundle) for p in candidates), f'Unresolved dependency: {binary}: {dependency}'
    output('/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', str(bundle))
    return {'bundle_id': info['CFBundleIdentifier'], 'version': version,
            'architectures': architectures, 'mach_o_files_checked': len(binaries)}
