# Verification — 7 September 2026

Target: Omarchy 4.0.2, native Wayland, Qt 6.11.2, GCC 16.2.1; Intel Core i7 (2 cores / 4 threads). No system packages installed. Sources and executable are in `/home/user/mindmap-qt-lab`.

## Tests

Final source rebuilt by `./scripts/test.sh` on the target using Qt offscreen:

| Suite | Passed | Failed |
| --- | ---: | ---: |
| Engine | 19 | 0 |
| Canvas events | 5 | 0 |
| Complete QML interface | 12 | 0 |

Counts include QtTest initialization and cleanup. The complete interface suite was additionally run under native Wayland: 12 passed, 0 failed. Logs are in `artifacts/tests-final.log` and `artifacts/tests-wayland-final.log`.

A slower, visible replay exercised the same interface in its own maximized window. That earlier full replay had 11 passes and one failure in the first editing-commit check; the unchanged isolated check subsequently passed (3 including setup/cleanup). The cause of that transient failure is unresolved. The later default-speed final suites passed. Do not treat the replay as proof of physical keyboard, trackpad, compositor focus or IME reliability. Run `./scripts/live-tests.sh` to watch again; avoid interacting with its window while it sends input.

Coverage includes hierarchy invariants, invalid import rejection, fold/undo, cached layout, manual subtree motion, cycle-safe reparenting, relationships, persistence, cursor-anchored zoom, keyboard creation/editing, rich-text persistence and preservation of rejected drafts. The prototype was also launched and captured on native Wayland/OpenGL (Qt graphics API 3).

## Measurements

Single runs on the target; these are observations, not statistically established performance guarantees.

| Nodes | Cold fixture creation | Cached horizontal | Cached vertical | Cached compact |
| --- | ---: | ---: | ---: | ---: |
| 1,000 | 66.80 ms | 0.90 ms | 0.84 ms | 0.47 ms |
| 10,000 | 435.39 ms | 16.34 ms | 12.78 ms | 8.68 ms |

Fixture creation includes nodes, text measurement and initial layout. Switching layouts reuses text measurements. All layouts still traverse the visible tree synchronously. See `artifacts/layout-final.json`.

The final complete-QML scripted pan run drew all 10,000 nodes at fit zoom, with text labels omitted at that scale. Across 121 samples after warm-up:

- Pan preparation, p95: **0.438 ms**.
- Scene update CPU time, p95: **0.026 ms**.
- Compositor-paced frame interval: **15.13 ms median**, **42.06 ms p95**.

This tests small pans inside retained overscan, where only the transform changes. It does not measure cold geometry upload, large viewport jumps, readable-text zoom, GPU execution time or end-to-end input latency. Frame pacing still has outliers; steady 60 FPS has not been established. See `artifacts/render-10000-retained.json`.

## Remaining scope

The macOS CMake build was subsequently verified with the new Qt 6.11.2 installation at `/Users/user/Qt/6.11.2/macos`: all three CTest suites passed (15.49 seconds total), and the application launched on Cocoa/Metal (graphics API 6). See `artifacts/macos-launch.log` and `artifacts/macos-demo.png`. The older Homebrew Qt has a missing ICU dependency and was not used. Windows remains unverified; Omarchy used qmake. Full MindNode parity, balanced left/right maps, images, production interchange, accessibility, autosave and packaging remain outside this sample. Undo uses bounded snapshots, and inactive text uses bounded raster caches. See the README for limits and controls.

## Theme extension verification

After the first four themes were added, fresh full suites passed on both hosts. Omarchy qmake results: **22 engine, 5 canvas, 15 UI passes**, zero failures. macOS CTest: **3/3 suites pass**, 11.17 seconds. These include theme catalog/order, depth appearance, branch ordinal inheritance, transactional JSON, undo/redo, keyboard activation of every theme card, rejection of theme switches with invalid drafts, pending-note preservation and PNG canvas background checks for every preset.

Native theme cases also passed on Cocoa/Metal and Wayland: 5 passes each including setup/cleanup. Logs: `artifacts/themes-ctest.log`, `themes-native-macos.log`, `themes-tests.log`, `themes-native-wayland.log`. Screenshots: `artifacts/themes-macos/` and `artifacts/themes-wayland/` (target copy). The application was relaunched on macOS with Beach Day and the Themes tab visible. Existing user document windows were not terminated.

Known limits: theme palettes/shapes are visual approximations with portable 15px default text; explicit rich-text colors remain preserved. Software rendering still simplifies connectors. The Mac logs a font-alias startup warning. No new steady-FPS claim or physical trackpad certification is made by this theme change. The separate PRD/gesture plan records those remaining tests.
