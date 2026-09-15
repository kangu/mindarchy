import contextlib
import io
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('release_github', Path(__file__).resolve().parents[1] / 'scripts/release-github.py')
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        output = contextlib.redirect_stdout(io.StringIO())
        output.__enter__()
        self.addCleanup(output.__exit__, None, None, None)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.dirs = {p: self.root / p for p in ('macos', 'windows', 'omarchy')}
        for directory in self.dirs.values():
            directory.mkdir()

    def mac(self, stamp='20260915', contents=b'installer'):
        folder = self.dirs['macos'] / '0.1.4' / 'arm64' / stamp
        folder.mkdir(parents=True)
        data = dict(version='0.1.4', architectures=['arm64'], signed=False, notarized=False)
        for key, extension, hashkey in [('package', 'pkg', 'sha256'), ('dmg', 'dmg', 'dmg_sha256')]:
            path = folder / f'Mindarchy-0.1.4-macos-arm64-unsigned.{extension}'
            path.write_bytes(contents)
            data[key], data[hashkey] = path.name, release.sha256(path)
        (folder / 'release.json').write_text(json.dumps(data))
        return folder

    def test_latest_complete_macos_and_excludes_other_files(self):
        self.mac('20260914', b'old')
        latest = self.mac()
        (latest / 'private.log').write_text('private')
        (latest.parent / '20260916').mkdir()
        assets = release.collect('0.1.4', ['macos'], self.dirs)
        self.assertEqual(len(assets), 2)
        self.assertTrue(all(a['path'].parent == latest for a in assets))

    def test_corrupt_macos_rejected(self):
        folder = self.mac()
        next(folder.glob('*.pkg')).write_bytes(b'corrupt')
        with self.assertRaisesRegex(release.ReleaseError, 'Checksum'):
            release.collect('0.1.4', ['macos'], self.dirs)

    def test_missing_platform_rejected(self):
        self.mac()
        with self.assertRaisesRegex(release.ReleaseError, 'windows'):
            release.collect('0.1.4', ['macos', 'windows'], self.dirs)

    def test_exact_version_and_linux_signatures(self):
        for name in ['Mindarchy-0.1.4-windows-x64-setup.exe', 'Mindarchy-0.1.40-windows-x64-setup.exe']:
            (self.dirs['windows'] / name).write_bytes(b'exe')
        package = self.dirs['omarchy'] / 'mindarchy-0.1.4-1-x86_64.pkg.tar.zst'
        package.write_bytes(b'arch')
        package.with_name(package.name + '.sig').write_bytes(b'signature')
        assets = release.collect('0.1.4', ['windows', 'omarchy'], self.dirs)
        self.assertEqual(len(assets), 3)
        self.assertFalse(any('0.1.40' in a['path'].name for a in assets))

    def test_ambiguous_linux_builds_rejected(self):
        for rel in (1, 2):
            (self.dirs['omarchy'] / f'mindarchy-0.1.4-{rel}-x86_64.pkg.tar.zst').write_bytes(b'arch')
        with self.assertRaisesRegex(release.ReleaseError, 'Multiple'):
            release.collect('0.1.4', ['omarchy'], self.dirs)

    def run_publish(self, existing=None, fail_upload=False, corrupt_download=False, resume=False, remote_names=None, make_public=True):
        asset = self.root / 'example.pkg'
        asset.write_bytes(b'installer')
        notes = self.root / 'notes.md'
        notes.write_text('Release notes')
        calls = []
        def gh(*args):
            calls.append(args)
            if args[0] == 'api' and '/assets?' in args[3]:
                return '\n'.join(json.dumps({'name': n}) for n in (remote_names or []))
            if args[0] == 'api':
                return json.dumps(existing) if existing else ''
            if args[:2] == ('release', 'upload') and fail_upload:
                raise release.ReleaseError('upload failed')
            if args[:2] == ('release', 'download'):
                dest = Path(args[args.index('--dir') + 1])
                (dest / asset.name).write_bytes(b'bad' if corrupt_download else asset.read_bytes())
            return ''
        with patch.object(release, 'gh', side_effect=gh):
            try:
                release.publish('kangu/mindarchy', 'v0.1.4', [asset], notes, make_public, True, resume)
            except release.ReleaseError:
                return calls, False
        return calls, True

    def test_default_draft_is_not_published(self):
        calls, success = self.run_publish(make_public=False)
        self.assertTrue(success)
        self.assertFalse(any('--draft=false' in c for c in calls))

    def test_main_stages_checksums_and_public_metadata(self):
        self.mac()
        def check_upload(repo, tag, files, notes, make_public, prerelease, resume):
            self.assertEqual(len(files), 4)
            by_name = {p.name: p for p in files}
            data = json.loads(by_name['release-assets.json'].read_text())
            self.assertEqual(data['version'], '0.1.4')
            self.assertTrue(all('path' not in a for a in data['assets']))
            for line in by_name['SHA256SUMS'].read_text().splitlines():
                digest, name = line.split('  ')
                self.assertEqual(digest, release.sha256(by_name[name]))
            self.assertIn('unsigned, not notarized', notes.read_text())
            self.assertFalse(make_public)
        with patch.object(release.shutil, 'which', return_value='/fake/gh'), patch.object(release, 'publish', side_effect=check_upload) as upload:
            self.assertEqual(release.main(['--version', '0.1.4', '--platforms', 'macos',
                                          '--macos-dir', str(self.dirs['macos'])]), 0)
            upload.assert_called_once()

    def test_publish_only_after_download_verification(self):
        calls, success = self.run_publish()
        self.assertTrue(success)
        operations = [c[:2] for c in calls]
        self.assertLess(operations.index(('release', 'download')), operations.index(('release', 'edit')))
        create = next(c for c in calls if c[:2] == ('release', 'create'))
        self.assertIn('--draft', create)
        self.assertIn('--verify-tag', create)

    def test_failed_upload_never_publishes(self):
        calls, success = self.run_publish(fail_upload=True)
        self.assertFalse(success)
        self.assertFalse(any(c[:2] == ('release', 'edit') for c in calls))

    def test_corrupt_download_never_publishes(self):
        calls, success = self.run_publish(corrupt_download=True)
        self.assertFalse(success)
        self.assertFalse(any(c[:2] == ('release', 'edit') for c in calls))

    def test_existing_public_release_is_untouched(self):
        calls, success = self.run_publish(existing={'tag_name': 'v0.1.4', 'draft': False}, resume=True)
        self.assertFalse(success)
        self.assertTrue(all(c[0] == 'api' for c in calls))

    def test_resume_skips_verified_existing_asset(self):
        calls, success = self.run_publish(existing={'id': 1, 'tag_name': 'v0.1.4', 'draft': True},
                                          resume=True, remote_names=['example.pkg'])
        self.assertTrue(success)
        self.assertFalse(any(c[:2] == ('release', 'upload') for c in calls))
        self.assertFalse(any(c[:2] == ('release', 'create') for c in calls))

    def test_resume_rejects_unexpected_asset(self):
        calls, success = self.run_publish(existing={'id': 1, 'tag_name': 'v0.1.4', 'draft': True},
                                          resume=True, remote_names=['private.log'])
        self.assertFalse(success)
        self.assertTrue(all(c[0] == 'api' for c in calls))

    def test_resume_rejects_changed_asset_without_upload(self):
        calls, success = self.run_publish(existing={'id': 1, 'tag_name': 'v0.1.4', 'draft': True},
                                          resume=True, remote_names=['example.pkg'], corrupt_download=True)
        self.assertFalse(success)
        self.assertFalse(any(c[:2] in [('release', 'upload'), ('release', 'edit')] for c in calls))

    def test_dry_run_never_calls_github(self):
        self.mac()
        with patch.object(release, 'gh', side_effect=AssertionError('network access')):
            result = release.main(['--version', '0.1.4', '--platforms', 'macos',
                                   '--macos-dir', str(self.dirs['macos']), '--dry-run'])
        self.assertEqual(result, 0)

    def test_existing_draft_requires_resume(self):
        calls, success = self.run_publish(existing={'tag_name': 'v0.1.4', 'draft': True})
        self.assertFalse(success)
        self.assertTrue(all(c[0] == 'api' for c in calls))


if __name__ == '__main__':
    unittest.main()
