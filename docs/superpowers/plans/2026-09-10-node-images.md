# Node Images Implementation Plan

> Execute inline with the executing-plans skill; preserve the current branch's existing work.

**Goal:** Portable compressed node images, drag/drop, proportional resizing, and verification on macOS and Omarchy.
**Architecture:** Dedicated header-only NodeImage value/codec shared by the engine, canvas, and preview. Node sizing includes a left image region; canvas owns temporary resize state and commits through engine undo.
**Tech Stack:** Qt 6 C++, Qt Quick/QML, PNG/JPEG, JSON.
**Spec:** ../specs/2026-09-10-node-images-design.md

## Constraints
- One embedded image per node; no source-path dependency.
- Preserve title wrapping, task controls, date controls, branch dragging, recovery and clipboard behavior.
- Compress imports to at most 2048 pixels on the longest side; retain alpha with PNG and use JPEG quality 88 for photographic content only when smaller.
- Bound source bytes/pixels and aggregate decoded memory; do not generate installers.

## Tasks
- [x] Add a failing engine test loading an image-bearing JSON fixture and asserting increased node size and preserved image on save.
- [x] Implement NodeImage codec with validation, compression, display geometry and JSON conversion. Add engine attach/resize/remove methods, one checkpoint per action, serialization, and aggregate limits.
- [x] Add canvas tests for actual drag/drop events, proportional edge resizing, cancellation, undo and invalid drops. Implement the corresponding QQuickItem event handlers and temporary preview geometry.
- [x] Integrate images in canvas labels, task/date hit targets, editing, PNG exports and Quick Look renderer. Add selected-image handles and image actions/preview in QML.
- [x] Run local engine/canvas/UI/preview regressions, fix failures, and rebuild macOS. Verify real UI using a disposable fixture.
- [x] Sync source to connected Omarchy, build with qmake6, run regressions and test the running Wayland app. Preserve existing unsaved windows during deployment.
- [x] Record results and compression measurements in README/progress; restart the latest macOS build gracefully.
