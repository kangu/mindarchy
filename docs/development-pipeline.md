# Development and release pipeline

## Findings and measured baseline

The September 20 macOS run spent 132.86 seconds in CTest, including 95.25 seconds in the offscreen UI suite. Native macOS application, window, and placement suites accounted for roughly another 31 seconds. Those native tests use Cocoa and can take focus or open dialogs. The main UI suite uses Qt's offscreen platform.

The UI suite has a 350 ms reset wait before each case, plus 81 explicit wait calls in its source and timed drag events. Some waits validate actual animations. Removing them indiscriminately would create timing-dependent failures.

Other avoidable work was present:

- Default CMake builds included every test executable, including repeated compilation of application resources and platform sources.
- `test.sh` and `live-tests.sh` used separate qmake directories, duplicated shared compilation, and diverged from the current CMake suite inventory.
- macOS installer builds use a separate release tree and perform deployment, signing and packaging. They are unsuitable for routine edit/build cycles.
- Re-running CMake configuration for every command adds Qt discovery overhead. Generated builds already detect when configuration inputs change.

## Development commands

```sh
python3 scripts/dev.py build
python3 scripts/dev.py check
python3 scripts/dev.py native
```

`build` builds the app and its required dependencies only. `check` builds and runs the fast offscreen suites, in parallel. `native` runs the opt-in native window-management suites (opens real windows). All reuse the existing CMake build tree. None of these launches or restarts the application.

Measured warm runs on this Mac after the changes:

| Operation | Wall time |
| --- | ---: |
| App build, no source changes | 3.1 s |
| Build fast test targets and run checks | 5.3 s |
| Fast tests alone | 2.63 s |
| Earlier full CTest run | 132.86 s |

These are warm measurements, not clean compilation estimates. Header edits still rebuild their dependents. The first configuration/build costs more.

Use `--qt /path/to/Qt` on initial setup, `--build-dir path` to choose a tree, and `--jobs N` to cap compilation concurrency (default: up to 8). macOS defaults to `build-macos`; other hosts use `build`. Windows multi-configuration builds use Release. `scripts/test.sh` delegates to the fast checks.

## When to run broader tests

- Documentation or script changes: syntax/configuration checks; no app restart.
- Engine, layout, rendering or serialization changes: fast checks.
- A specific QML interaction change: build `ui_test` and run the relevant QtTest function with `QT_QPA_PLATFORM=offscreen`. Example on macOS: `QT_QPA_PLATFORM=offscreen build-macos/ui_test pastedFontSizesFollowNodeDepth`.
- Broad QML changes: `python3 scripts/dev.py ui` (offscreen).
- Release validation or requested native-window diagnosis: `python3 scripts/dev.py full` (fast+offscreen, no flicker); `python3 scripts/dev.py native` for the window-management suites (opens real windows).
- Requested visual replay only: `python3 scripts/dev.py live` or `scripts/live-tests.sh`.

Do not run the full suite after every edit. Native tests have a shared CTest desktop lock so they cannot race each other if someone enables parallel execution. They still need a real desktop to exercise OS behavior.

## CMake and release behavior

Tests are excluded from the default build. Explicit targets are `mindarchy-tests-fast` and `mindarchy-tests`. `BUILD_TESTING=OFF` disables test configuration and the Qt Test requirement altogether.

For direct CMake usage:

```sh
cmake --build build --target mindarchy-tests-fast --parallel
ctest --test-dir build -L '^fast$' --parallel 4 --output-on-failure

# Full release validation; build all test binaries first.
cmake --build build --target mindarchy mindarchy-tests --parallel
ctest --test-dir build -LE '^native$' --output-on-failure
ctest --test-dir build -L '^native$' --output-on-failure
```

The macOS release script explicitly builds every test and runs CTest in two phases: the fast+offscreen tier (`-LE '^native$'`) then the opt-in native tier (`-L '^native$'`) before packaging. Windows does the same by default; its pre-existing `SkipTests` override skips both legs. Omarchy's makepkg/qmake packaging path does not currently run CTest: run `dev.py full` (plus `dev.py native`) on the Linux release host as a separate release gate. GitHub publishing uploads existing artifacts and does not certify their tests.

Separate platform/architecture release trees remain appropriate. Installer staging and clean makepkg builds favor reproducible packages rather than incremental development speed.

## Further improvements worth measuring

1. Replace individual UI waits with observable completion conditions, preserving explicit animation tests. Measure per-case runtime before changing timing behavior.
2. Evaluate Ninja for a fresh build directory; the current Makefiles tree is retained to avoid a forced rebuild. Ninja and a compiler cache were not available on this Mac during this audit.
3. Consider compiler caching for clean builds and CI, after measuring actual compilation costs. It will not accelerate deliberate UI waits.
4. Add hosted per-platform release checks, particularly a formal Linux gate, so native tests do not occupy the development desktop. No CI workflows were present in this repository during the audit.
