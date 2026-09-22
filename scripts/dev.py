#!/usr/bin/env python3
"""Incremental app builds and opt-in test tiers, sharing one CMake build tree."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', nargs='?', choices=['build', 'check', 'ui', 'native', 'full', 'live'], default='build',
                        help='build: app only; check: quiet fast tests (12); ui: offscreen UI; native: opt-in window-management suites (opens real windows); full: release checks without native flicker (fast+offscreen only); live: visible UI replay')
    parser.add_argument('--build-dir', type=Path, default=ROOT / ('build-macos' if sys.platform == 'darwin' else 'build'))
    parser.add_argument('--qt', help='Qt installation prefix; needed only if CMake cannot find Qt')
    parser.add_argument('--jobs', type=int, default=min(8, os.cpu_count() or 2))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    build = args.build_dir.resolve()
    started = time.monotonic()

    def run(command):
        print('+ ' + ' '.join(map(str, command)), flush=True)
        subprocess.run(list(map(str, command)), cwd=ROOT, check=True)

    configure = ['cmake', '-S', ROOT, '-B', build, '-DBUILD_TESTING=ON']
    if not (build / 'CMakeCache.txt').exists():
        configure += ['-DCMAKE_BUILD_TYPE=Release']
    if args.qt:
        configure += [f'-DCMAKE_PREFIX_PATH={args.qt}']
    cache = build / 'CMakeCache.txt'
    # CMake's generated build already reruns configuration when inputs change.
    # Avoid repeating Qt discovery on every no-op development command.
    if not cache.exists() or args.qt or 'BUILD_TESTING:BOOL=ON' not in cache.read_text():
        run(configure)
    targets = {'build': ['mindarchy'], 'check': ['mindarchy-tests-fast'],
               'ui': ['ui_test'], 'native': ['mindarchy-tests-native'],
               'live': ['ui_test'], 'full': ['mindarchy', 'mindarchy-tests']}
    run(['cmake', '--build', build, '--config', 'Release', '--target', *targets[args.mode], '--parallel', args.jobs])
    if args.mode == 'live':
        os.environ.setdefault('MINDMAP_LIVE_TEST_MS', '1600')
        binary = build / ('ui_test.exe' if sys.platform == 'win32' else 'ui_test')
        if not binary.exists():
            binary = build / 'Release' / binary.name
        run([binary])
    elif args.mode == 'native':
        run(['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure', '--no-tests=error',
             '-L', '^native$'])
    elif args.mode != 'build':
        command = ['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure', '--no-tests=error']
        if args.mode == 'check':
            command += ['-L', '^fast$', '--parallel', min(args.jobs, 4)]
        elif args.mode == 'ui':
            command += ['-R', '^ui$']
        elif args.mode == 'full':
            command += ['-LE', '^native$']
        run(command)
    print(f'{args.mode} completed in {time.monotonic() - started:.1f}s', flush=True)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
