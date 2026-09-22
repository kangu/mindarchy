# 2026-09-21T21:22:56+03:00 — macOS test tiering Task 3

## Initial Prompt
Implement Task 3 of the macOS test tiering plan in qt-prototype (branch codex/evaluate-sharing): add `dev.py native` mode (target `mindarchy-tests-native`, `-L '^native$'`), make `full` filter out native tests with `-LE '^native$'`, keep live/check/ui unchanged; release-macos.py and release-windows.ps1 run two CTest legs (fast+offscreen then native, `SkipTests` skips both on Windows); update docs/development-pipeline.md tier statements and the AGENTS.md policy bullet. Commit `feat: no-flicker full gate with opt-in native tier`. Report to .superpowers/sdd/tiering-task-3-report.md.

## Plan
1. scripts/dev.py: argparse choices + help text; targets map gains native; dedicated native ctest branch before generic branch; full gains -LE '^native$'.
2. scripts/release-macos.py :82: single ctest -> two legs (-LE then -L) with --no-tests=error.
3. scripts/release-windows.ps1 :55-57: two ctest legs inside !SkipTests block, matching existing array-composition style.
4. Docs + AGENTS.md tier statements refreshed (native opt-in; full = fast+offscreen no-flicker; release runs both legs).
5. Verify full/check/build/--help/py_compile; commit.

## Implementation Summary
- All edits landed as specified, no new code comments.
- Pre-existing branch breakage found and fixed: `application_test` and `macapplication_test` compile `src/macapplication.cpp`, which now includes `sharecoordinator.h` (sharing work), but neither target linked `mindmap_collaboration` (only `mindarchy` did) -> fatal include error under `full`. Fix: added `mindmap_collaboration` to both targets' `target_link_libraries` in CMakeLists.txt (PUBLIC include dirs src, src/collaboration; also supplies coordinator symbols for linking).
- Verification:
  - `python3 scripts/dev.py full`: 12/12 passed with `-LE '^native$'` — offscreen only, no visible windows, no ui flake on the run. Label summary: fast=22.50s (11 tests), offscreen=119.30s (12), slow=96.80s (ui). **Wall time 146.2 s.**
  - `python3 scripts/dev.py --help`: new mode text shown; choices build,check,ui,native,full,live.
  - `python3 scripts/dev.py build`: warm no-op, 14.8 s, `Built target mindarchy`.
  - `python3 scripts/dev.py check`: 11/11 fast tests, 11.33 s ctest.
  - `python3 -m py_compile scripts/dev.py scripts/release-macos.py`: OK.
  - release-windows.ps1 changed legs (not executable here): `Run ctest @('--test-dir', $BuildDir, '--output-on-failure', '--no-tests=error', '-LE', '^native$')` then `-L '^native$'`, both inside `if (!$SkipTests)`.
- Commit: 045254d (6 files: dev.py, release-macos.py, release-windows.ps1, docs/development-pipeline.md, AGENTS.md, CMakeLists.txt).
- Did not run dev.py native; app did not need a restart (no app-source change; test/CMake/CI-only).

## Next Steps
- Task 4 per the tiering plan: release-flow validation and any remaining native-tier polish on other platforms.
