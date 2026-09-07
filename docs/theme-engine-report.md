# Theme engine implementation report

Implemented the bounded theme catalog, node appearance rules, Engine integration, persistence, history, and tests described in `theme-engine-brief.md`.

## Delivered

- Added `src/theme.h` and `src/theme.cpp` with the required `NodeShape`, `NodeAppearance`, `MapTheme`, and `Themes` interfaces.
- Added the ordered Beach Day, Holographic, Retro, Arcade, and legacy Lab catalogs. Catalog maps expose `id`, `name`, `canvas`, and a six-color `QVariantList` palette.
- Added `themeId`, `themes`, and `canvasColor` Engine properties, plus typed `Engine::appearance(int)` lookup.
- Theme changes create one history checkpoint. Duplicate IDs do nothing; unknown IDs leave state unchanged and set an error.
- Theme state participates in undo/redo without changing text, hierarchy, layout settings, manual offsets, selection, notes, tasks, or relationships.
- Each rebuild caches the top-level branch ordinal for every node. Appearance lookup uses depth and this ordinal, and reparenting refreshes it.
- Saved documents include additive `themeId` while retaining numeric format version 1. Documents without the field load as Lab. Unknown explicit IDs fail before state mutation.
- Updated qmake and CMake build entries for the new theme source files.

## TDD and verification

The new tests were compiled before implementation and failed on the missing theme interfaces. After implementation, the dedicated build was freshly verified with:

```sh
/Users/user/Qt/6.11.2/macos/bin/qmake ../tests/engine_test.pro
make -j2
QT_QPA_PLATFORM=offscreen ./engine_test
```

Result: 22 passed, 0 failed, 0 skipped. The only runtime output outside test results is the pre-existing Qt missing `Sans-serif` alias warning.
