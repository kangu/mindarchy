# macOS test re-tiering: no-flicker full gate, opt-in native tier — design

Date: 2026-09-21
Status: Approved design (sections approved interactively)
Scope: `CMakeLists.txt` test section, `scripts/dev.py`, release scripts, `AGENTS.md`, `docs/development-pipeline.md`

## Goal

`dev.py full` never opens visible windows (no screen flicker) and stays the standard pre-merge validation; the four window-opening native-cocoa suites move to a new opt-in `dev.py native` tier; release packaging still runs both so release coverage is unchanged.

## Background facts (from the measured inventory)

- Registered macOS tests: 15. Flicker sources (cocoa QPA, real windows): `macapplication`, `macwindow`, `windowplacement` (zoom cases), `application` — ~31 s wall, serialized by `RESOURCE_LOCK native_desktop` (`CMakeLists.txt:192-198`).
- No-flicker heavy cost: offscreen `ui` suite ~95 s (fixed per-case waits), already excluded from `check`.
- `collaboration_store` and `collaboration_engine_bridge` have no label/timeout — invisible to `-L ^fast$` and to every tier; they only run in an unfiltered ctest.
- Full baseline pre-optimization: 132.86 s (95 s = offscreen `ui`, 31 s native, rest fast).
- Release gates run full unfiltered ctest (`scripts/release-macos.py:81-82`, `scripts/release-windows.ps1:56-57`); dev policy (AGENTS.md): `check` default, `ui` for broad QML, `full` reserved for release validation.

## Tier model (authoritative)

| Tier (dev.py mode) | Build targets | ctest invocation | Flicker | Intent |
|---|---|---|---|---|
| `build` | `mindarchy` | — | no | app only (unchanged) |
| `check` (default) | `mindarchy-tests-fast` | `-L '^fast$' --parallel ≤4` | no | dev loop (unchanged) |
| `ui` | `ui_test` | `-R '^ui$'` | no | broad QML replay (unchanged) |
| `native` (NEW) | `mindarchy-tests-native` | `-L '^native$'` | YES | opt-in window-management gate |
| `full` | `mindarchy` + `mindarchy-tests` | `-LE '^native$'` (everything except native) | no | standard pre-release validation |
| `live` | `ui_test` (visible, `MINDMAP_LIVE_TEST_MS`) | direct binary run | by intent | unchanged |

Release scripts: `scripts/release-macos.py` (and Windows equivalent `release-windows.ps1`) run the `full` set (filtered `-LE '^native$'`) AND then the native leg (`-L '^native$'`) as two explicit sequential ctest invocations — release coverage is total; flicker is isolated to the native leg. Docs claim in `docs/development-pipeline.md` updated to match. AGENTS.md policy section updated to describe both tiers and that `native` is opt-in for developers but mandatory before packaging.

## Criticality verdicts (evidence in the exploration ledger)

Kept native (earn their screen time, no offscreen equivalent):
- `macwindow` — 120 animated native `setFrame:` resizes, native close sheets, native Help/shortcuts window (`tests/macwindow_test.mm:24,30-63,130-181`). No offscreen substitute.
- `macapplication` — real `NSWindow` tab-button alignment, native zoom, recover-across-process incl. minimize animation (`tests/macapplication_test.mm:21,59,212-215`).
- `application` — app-level flows whose meaning only holds with real windows (`switchingTabsDoesNotResizeHost` with `showMaximized`; recover flows). Flagged as a future offscreen-conversion candidate, NOT moved now.
- `windowplacement` — native-only subset (see split below); the AppKit zoom regression is the fixture's whole reason to exist (`CMakeLists.txt:110` comment).

Moved offscreen (flicker eliminated without coverage loss):
- From `windowplacement_test`, three slots become a new offscreen executable `windowplacementoffscreen_test`: `geometryValidation` (`:203`), `panelVisibilitySurvivesRestartAndResize` (`:36`), `nativeHyprlandRestore` (`:153` — skips on macOS today; runs on non-mac like before). New file `tests/windowplacementoffscreen_test.cpp` with those three slots; they never show windows, verified in inventory.
- Label it `fast;offscreen`, appended to `fast_tests`, TIMEOUT 60.

Label debt fixed:
- `collaboration_store` and `collaboration_engine_bridge` get `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"`, `LABELS "fast;offscreen"`, `TIMEOUT 60`, and join `fast_tests`/`fast_test_targets` so they run in `check` (and thus at every dev loop and at `full`).

## Constraints

- No test function's assertions change; regrouping only (mechanical slot moves + one new file).
- `maczoom_test_support.mm` / `src/macplacement.mm` remain linked ONLY to the native `windowplacement_test`; the offscreen split must not link AppKit sources it doesn't need.
- `mindarchy-tests` aggregate target still depends on all test binaries (release build target unchanged; EXCLUDE_FROM_ALL unchanged).
- `dev.py full` stays release-validation usable on the dev machine: no visible window may appear during `full` (asserted via the ctest filter, not code changes).
- Windows native test `windowsdialogs` is already opt-in via label; the new `-LE native` filter keeps Windows release behavior right once release-windows.ps1 runs native leg too (it already runs unfiltered ctest; update to the same two-phase filter for consistency).

## Success criteria

1. `dev.py full` completes with zero windows created on screen (only offscreen QPA processes); measured wall time reported before/after in the task summary.
2. `dev.py native` runs the four cocoa suites with their current assertions unchanged (~31 s).
3. `python3 scripts/dev.py check` unchanged in spirit and still at ~28 s (now including the two newly-labeled collaboration suites).
4. `scripts/release-macos.py` runs both legs sequentially and fails on either.
5. `collaboration_store` and `collaboration_engine_bridge` participate in `check` and (unfiltered) full ctest with proper labels/timeouts.
