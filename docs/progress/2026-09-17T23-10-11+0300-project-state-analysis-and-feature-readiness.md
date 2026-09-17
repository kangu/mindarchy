# Project state analysis and feature readiness

## Initial Prompt
Analyse the current project state thoroughly considering all aspects involved, review the past work from /docs folders and prepare to implement work on features.

## Plan
1. Survey the repository layout, README, and AGENTS.md conventions.
2. Read docs/ (plan, controls, verification, platform and theme docs, GitHub release guide, mobile PRD) and all 94 progress notes with emphasis on the most recent work.
3. Inspect the source tree (engine, canvas, QML, platform layers), CMake test targets, and release tooling state.
4. Reconcile the implemented feature set against the MindNode Classic evaluation backlog and collect outstanding next steps from progress notes.
5. Report findings without changing application code; no rebuild required.

## Next Steps
Release completion first, then feature work in the recommended order below:
1. Rebuild the Windows 0.1.4 installer on the Windows machine (sync the tree, run `release-windows.ps1 -Version 0.1.4`, confirm the fixed offscreen `application_test` passes).
2. Rebuild the macOS 0.1.4 installers — the staged DMG/PKG (20260916T1818Z) predates the traffic-light, keep-zoomed-windows, and window-placement fixes now at HEAD.
3. Stage installers and publish with `python3 scripts/release-github.py --version 0.1.4` (auto-tags `v0.1.4`; consider `--tag-commit` of the build commit for provenance, or rebuild all platforms from HEAD).
4. Feature queue (MindNode evaluation priority order, minus completed items): export (PDF/SVG/OPML/Markdown, transparent PNG, copy-as-image), visual tags and filtering, balanced layout plus branch commands (insert parent, sort siblings, indent/outdent, fold completed), Quick Entry, user themes/reusable templates, version history; small wins: zoom to selection, find and replace.

## Implementation Summary
Analysis only; no application code changed and no rebuild performed.

**Architecture (verified against source):** Qt Quick + C++20 desktop mind mapper targeting Omarchy first, then macOS and Windows 11. `src/engine.cpp` (~1,800 lines) holds the document model (indexed ordered tree, tasks with progress, calendar/date nodes with numeric sums, meeting/weekly templates, per-node styles, themes, branch clipboard, search, links/resources, node images, versioned JSON `.omm` persistence, recovery, recents). `src/canvas.cpp` (~1,560 lines) is a custom scene-graph canvas with live marquee selection, drag reparenting, creation handles, focus mode with breadcrumb, search highlighting, image selection/resize, and PNG export. `qml/` (~2,400 lines, `DocumentWorkspace.qml` the largest at 892) provides the shell, tabs, and inspectors. Platform layers: `macwindow.mm`/`macapplication.cpp` (native chrome, traffic lights, window cycling/zoom retention, native Help shortcuts), `windowplacement` (cross-platform persistence incl. Hyprland), `windowsdialogs` + `WindowMenuBar` (Windows-only Alt menu), `shelltheme` (Omarchy live theme), Quick Look preview extension. CMake is the primary build (10+ test suites: engine, canvas, ui, application, macapplication, macwindow, windowplacement, preview, manualplacement, windowsdialogs, plus Python publisher tests); qmake `.pro` retained for the Omarchy build path.

**Docs and process:** every task is recorded in `docs/progress/` (94 notes) with Initial Prompt / Plan / Next Steps / Implementation Summary; AGENTS.md additionally requires macOS rebuilds after restart-worthy tasks and keeping the shortcut tables (`src/macwindow.mm`, `qml/KeyboardShortcuts.qml`) in sync. Verification history lives in `docs/verification.md`; the last full 9-suite CTest pass was the shared-tabs change (Sept 14); since then fixes ran focused suites, with the full suite due on the next-but-one feature change per the noted cadence.

**Current state:** git clean at `c62feff` (Sept 17, 21:58), remote `kangu/mindarchy`, no local tags (publisher creates them). A 0.1.4 GitHub release is in flight: Omarchy x86_64 package built (Sept 17 14:28, `artifacts/omarchy/`), macOS arm64 DMG+PKG staged but built Sept 16 18:18 — it predates the traffic-light click fix, the Cmd+` zoom-retention fix, and the window-placement/tab fixes; the Windows installer is still 0.1.0, with the 0.1.4 rebuild pending on the Windows machine after the Alt+F/Alt+T menu-bar fix and the offscreen font-dir test fix (both in `c62feff`). None of the staged installers match HEAD, so either rebuild all platforms from HEAD or pin `--tag-commit` to the actual build commit when publishing.

**Feature completeness vs the MindNode Classic evaluation (docs/progress 2026-09-09T18-46-11):** implemented since that review — branch clipboard, focus mode, links/resources, node images, shared tabs, live marquee, template popover, plus the earlier tasks/notes/calendars/themes/manual placement. Still open, in the evaluation's priority order: (3) full-map/selection export (PDF/SVG, transparent PNG, copy-as-image, OPML/Markdown) — only viewport PNG exists; (4) visual tags and filtering; (7) balanced automatic layout and branch commands — verified absent from `engine.h`: insert parent, sort siblings, indent/outdent, fold completed, find/replace, zoom to selection; (8) Quick Entry; (9) user themes and reusable branch templates; (10) version history. Node-to-node and cross-document links remain future additions to the existing resources feature. The mobile PRD remains a proposal with no implementation scheduled. Cleanup candidate: the stale `.worktrees/shared-document-tabs` worktree sits at the already-merged commit `e56b87c`.
