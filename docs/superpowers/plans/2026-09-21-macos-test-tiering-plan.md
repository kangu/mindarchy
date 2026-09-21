# macOS Test Re-Tiering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `dev.py full` becomes flicker-free (fast + offscreen QML only); the four native-cocoa suites move to an opt-in `dev.py native` tier; release scripts run both legs sequentially so release coverage is unchanged.

**Architecture:** No logic-test changes — regrouping only. CMake label/membership changes create `mindarchy-tests-native`; `windowplacement` splits into native (zoom/AppKit crisp cases) + new offscreen `windowplacementoffscreen` (never shows windows); two collaboration suites finally get labels/timeout; dev.py gains the `native` mode and `full` filters native out; release-macos.py / release-windows.ps1 run `-LE '^native$'` then `-L '^native$'`.

**Spec:** `docs/superpowers/specs/2026-09-21-macos-test-tiering-design.md` (authoritative; tier table + criticality verdicts).

**Tech stack:** CMake ctest labels, Python dev runner, existing test sources.

## Global Constraints

- No test assertions change; only file slot moves, new labels, runner wiring.
- `maczoom_test_support.mm` / `src/macplacement.mm` stay linked only to native `windowplacement_test`.
- No code comments in new code (repo style).
- Verification via `python3 scripts/dev.py build/check` + ctest filters; `dev.py native` (flicker) only inside its own task step; `dev.py ui` NOT required for CMake-only tasks (regression check is `check`).
- The user's running app must not be killed except where needed for test isolation (none expected).
- Commit after each task.

---

### Task 1: CMake tier rewiring (labels + targets)

**Files:**
- Modify: `CMakeLists.txt` (test section, lines ~100-223)

**Interfaces:**
- Produces: `native_tests` list: `set(native_tests application macwindow macapplication windowplacement)` on Apple (Windows branch: `windowsdialogs` stays as `native_tests` member too), aggregate `add_custom_target(mindarchy-tests-native DEPENDS ${native_targets})`, label property `LABELS "slow;native"`, `RESOURCE_LOCK native_desktop` for the native set (already exists per-Apple — consolidate into single property block).

Exact edits:
1. After the `slow_tests` definitions, add `set(native_tests)`; on APPLE: `list(APPEND native_tests macapplication macwindow windowplacement application)`; on WIN32: `list(APPEND native_tests windowsdialogs)`. Add `set(native_targets)` and in the build-target loop include `native_tests` (build `${test}_test` with EXCLUDE_FROM_ALL and add to `${native_targets}`), plus `add_custom_target(mindarchy-tests-native DEPENDS ${native_targets})`.
2. `fast_tests`/`slow_tests` redefinition: remove `windowplacement` and `application` from `slow_tests` (their native membership replaces slow listing); keep `ui` in `slow_tests` alone so `LABELS "slow"` still applies. `windowsdialogs` stays WIN32 `slow_tests` too (both labels fine).
3. Replace the APPLE/elseif(Win32) label block with:
```cmake
set_tests_properties(${native_tests} PROPERTIES LABELS "slow;native" RESOURCE_LOCK native_desktop)
```
(keep TIMEOUTs already set on application; macwindow currently has no explicit TIMEOUT — leave untouched per "no assertion changes" but acceptable to add TIMEOUT 45? NO — leave as is.)
4. `collaboration_store` and `collaboration_engine_bridge`: add `set_tests_properties(collaboration_store collaboration_engine_bridge PROPERTIES LABELS "fast;offscreen" TIMEOUT 60)` and append both names to `fast_tests`.
- ⚠ Do NOT restructure anything else.

**Verify:**
- `cmake -S . -B build-macos -DBUILD_TESTING=ON` configures; `ctest --test-dir build-macos -C Release -N` (test count unchanged at 15); `ctest -N -L '^fast$'` now shows 10 tests (adds collaboration_store, collaboration_engine_bridge); `ctest -N -L '^native$'` shows exactly macapplication, macwindow, windowplacement, application.
- `python3 scripts/dev.py check` green.

**Commit:** `build: label native and collaboration test suites for tiered runs`

---

### Task 2: windowplacement offscreen split

**Files:**
- Create: `tests/windowplacementoffscreen_test.cpp`
- Modify: `CMakeLists.txt` (new executable + fast_tests entry; remove three slots' responsibilities from native counting)

**Step 1: Write `tests/windowplacementoffscreen_test.cpp`** — a QObject with slots `geometryValidation`, `panelVisibilitySurvivesRestartAndResize`, `nativeHyprlandRestore` copied VERBATIM from `tests/windowplacement_test.cpp` (same includes: `../src/windowplacement.h`, `QQuickWindow`, `QQmlEngine`, `QQmlComponent`, `QJsonDocument`, `QJsonArray`, `QJsonObject`, `QScreen`, `QSettings`, `QTemporaryDir`, `QtTest`; note `panelVisibilitySurvivesRestartAndResize`'s inline QML snippets and `panelVisibilitiesMatch`/helpers must come along — copy any private helpers/`std::unique_ptr` usage and the includes faithfully), with `QTEST_MAIN` + `#include "windowplacementoffscreen_test.moc"`.
**Step 2:** Remove those three slots from `tests/windowplacement_test.cpp` (keep all others).
**Step 3: CMake:** after the `windowplacement_test` block add:
```cmake
add_executable(windowplacementoffscreen_test tests/windowplacementoffscreen_test.cpp src/windowplacement.cpp)
target_link_libraries(windowplacementoffscreen_test PRIVATE Qt6::Quick Qt6::Test)
add_test(NAME windowplacementoffscreen COMMAND windowplacementoffscreen_test)
set_tests_properties(windowplacementoffscreen PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```
Append `windowplacementoffscreen` to `fast_tests` (gets `LABELS fast;offscreen TIMEOUT 60` via the fast block).
**Step 4:** The macOS-only `QT_QPA_FONTDIR` APPEND at the WIN32 block: leave the existing windowplacement APPEND untouched.

**Verify:** `ctest --test-dir build-macos -C Release -R 'windowplacement' -N` shows both tests; run `-R 'windowplacementoffscreen'` (offscreen, fast label) GREEN; `-R '^windowplacement$'` NOT run here (native — runs in Task 4).
**Commit:** `test: split windowplacement offscreen slots from native zoom suite`

---

### Task 3: dev.py `native` mode, `full` filter, release scripts

**Files:**
- Modify: `scripts/dev.py` (mode choices + tier map + help text), `scripts/release-macos.py`, `scripts/release-windows.ps1`, `docs/development-pipeline.md`, `AGENTS.md` test-policy section

**dev.py (exact edit):** `choices=['build','check','ui','full','live']` → add `'native'`. Help text line updated to document the new tier. Target map: `'native': ['mindarchy-tests-native']`. After `run(cmake --build ...)`:
```python
if args.mode == 'live': ...unchanged
elif args.mode == 'native':
    run(['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure', '--no-tests=error', '-L', '^native$'])
elif args.mode != 'build':
    command = ['ctest','--test-dir',build,'-C','Release','--output-on-failure','--no-tests=error']
    if args.mode == 'check': command += ['-L','^fast$','--parallel',min(args.jobs,4)]
    elif args.mode == 'ui': command += ['-R','^ui$']
    elif args.mode == 'full': command += ['-LE','^native$']
```
Also update the argparse `help=` string accordingly (native: `opt-in window-management suites (opens real windows); full: release checks without native flicker`).
**release-macos.py (exact edit at :81-82):**
```python
run(['cmake', '--build', build, '--target', 'mindarchy', 'mindarchy-tests', '--parallel', '4'])
run(['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error', '-LE', '^native$'])
run(['ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error', '-L', '^native$'])
```
**release-windows.ps1 (same two-phase pattern at :56-57)**, preserving the `SkipTests` flag semantics (skipping skips both legs).
**docs/development-pipeline.md:** update the tier table (native opt-in, full no-flicker) + line 44 statement; AGENTS.md: `Reserve dev.py full` sentence → `dev.py full = fast+offscreen validation; dev.py native = opt-in native window suites run before release packaging (release scripts run both).`
**Verify:** `python3 scripts/dev.py full` completes flicker-free and green (this runs the full offscreen suite, ~100 s, quote time); `ctest --test-dir build-macos -C Release -L '^native$'` still passes once locally in Task 4 only (combined step below to avoid double-flicker).
**Commit:** `feat: no-flicker full gate with opt-in native tier`

---

### Task 4: native leg verification + progress notes

**Files:**
- Modify: `docs/progress/<timestamp>-macos-test-tiering.md` (AGENTS.md-required summary: Initial Prompt / Plan / Implementation Summary / Next Steps)

- [ ] **Step 1:** Run `python3 scripts/dev.py native` once (flickering is INTENDED here; quote per-test times) — all 4 native suites green (windowplacement now has 6 native slots after the Task 2 split).
- [ ] **Step 2:** Run `python3 scripts/dev.py full` — quote wall time vs the 132.86 s baseline and confirm zero visible windows appeared (all offscreen).
- [ ] **Step 3:** Run `python3 scripts/dev.py check` — quote.
- [ ] **Step 4:** Commit docs summary `docs: test tiering progress notes`.

---

## Self-review

- Spec coverage: tier table (T1-T3), label debt (T1), windowplacement split (T2), release two-phase (T3), success criteria measurement (T4). ui-suite per-case wait optimization explicitly out of scope (spec says follow-up).
- Placeholders: none.
- Type consistency: label regexes `-L '^native$'` vs `LABELS "slow;native"` used consistently; `mindarchy-tests-native` aggregate name matches dev.py target.
