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


## Date node extension — 8 September 2026

macOS Qt 6.11.2: all four CTest suites passed after the Date changes. The dedicated native Cocoa Date interaction test also passed (3 checks including initialization and cleanup). Evidence: `artifacts/date-tests.log`, `artifacts/date-native.log`, and screenshots in `artifacts/date-macos/`.

Coverage includes current-period creation, Monday-first weeks crossing years, leap-year month padding, entry add/edit/remove, Cancel, filled-day hover text, period arrow clicks, inspector Week/Month switching, drag-versus-click behavior, unchanged geometry on entry edits, undo/redo, save/open and invalid calendar data rejection. These use synthesized Qt input; physical device gestures and Windows have not been verified.

Omarchy Qt 6.11.2: rebuilt application and all three offscreen suites passed (29 engine, 6 canvas, 21 UI checks). The dedicated Date interaction test also passed in native Wayland (3 checks). The first full run stopped on a missing research-theme example fixture; syncing the existing examples resolved that setup failure. Logs: `artifacts/date-omarchy-tests.log` and `artifacts/date-native-wayland.log`.


### Inspector type conversion

Added a Text/Date selector above Task in the Node inspector. Fresh macOS validation: four CTest suites passed; the dedicated native inspector-conversion test passed. Tests verify in-place conversion, retained title/children, task reset with undo restoration, and retained calendar entries after switching to Text, saving, reopening and switching back to Date. Logs: `artifacts/date-type-tests.log` and `artifacts/date-type-native.log`.

Omarchy type-conversion validation passed: 30 engine, 6 canvas and 22 UI checks. The Date toolbar action was subsequently removed so the Node inspector is the user-facing type entry point. The final macOS build passes all four suites. See `artifacts/date-type-omarchy.log` and `artifacts/date-type-tests.log`.


### Date numeric sums

macOS: all four CTest suites passed, and the native Date interaction test passed (3 checks including setup/cleanup). The screenshot `artifacts/date-sums-macos/month-sums.png` was visually checked for row alignment, negative/decimal totals and the monthly footer. Calculation tests cover mixed text/numeric entries, zero, decimal separators, month boundary exclusions, week boundary inclusions, renderer cache invalidation, geometry changes, and undo/redo. Evidence: `artifacts/date-sums-tests.log` and `artifacts/date-sums-native.log`.

Omarchy numeric-sum validation: rebuilt application; 31 engine, 6 canvas and 22 UI checks passed. Native Wayland Date interaction test: 3 passed, zero failures. Logs: `artifacts/date-sums-omarchy.log` and `artifacts/date-sums-wayland.log`.


### Three-way node type selector

The Node panel now reuses MapOptionBar for Text, Task and Date icons, including hover/focus/selected styling and arrow-key navigation. Type-specific controls share its bordered container. macOS: four CTest suites passed; the native selector test passed (3 checks). Visually inspected `artifacts/node-types-macos/task.png` and `date.png`. The test checks Task completion, Date conversion, undo restoring a completed task, Text conversion preserving the title, and hiding extra options for Text. Evidence: `artifacts/node-types-tests.log`, `artifacts/node-types-native.log`.

## Window placement persistence — 8 September 2026

macOS: all five CTest suites passed; the native Cocoa placement test passed (four checks, with the Hyprland-specific case skipped). Tests cover saved geometry restoration, missing-display and off-screen fallback, invalid dimensions and small screens.

Omarchy: full offscreen engine/canvas/UI/placement suites passed. The native Hyprland 0.56.2 test passed (four checks, with the Cocoa case skipped), including exact floating rectangle restoration, missing-monitor fallback and retaining tiled mode. The newer compositor requires Lua dispatchers; capability detection selects these while keeping a legacy fallback.

The complete Omarchy application was also launched twice under an isolated test preference directory, gracefully closed between launches, and reported `[2010,270]` position and `[820,660]` size on both launches with floating mode retained. The test deliberately omitted `HYPRLAND_INSTANCE_SIGNATURE` to verify automatic discovery of the matching Wayland session. No global compositor rules or existing user documents were changed.

Evidence: `artifacts/placement-tests.log`, `placement-native-macos.log`, `placement-omarchy.log`, `placement-native-wayland.log`, `placement-app-wayland.log`. Missing displays were simulated through saved placement data and synthetic screen layouts; no physical display was unplugged. Legacy Hyprland, X11, and Windows were not exercised.
