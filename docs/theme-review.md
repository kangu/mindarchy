# Theme implementation review

Bounded source review of the first four themes against `themes-plan.md` and `theme-engine-brief.md`. Reviewed the catalog, Engine theme state/history/JSON validation and branch resolution, canvas GPU/software shape and text-cache paths, Themes inspector, draft guard and new tests. Runtime tests/screenshots are handled separately by the root task. No application source files were changed in this review.

## Scoped follow-up: both findings resolved

- **Pending notes preserved** — `qml/Main.qml:282–292` now tracks the last loaded selected ID and stored notes. `syncNotes()` leaves the field untouched when a theme emits `changed` without changing either value. `tests/ui_test.cpp:115–124` covers retention of a pending draft and synchronization when selecting another node. Root reports this regression failed before the fix and passes after it.
- **Retro fourth-branch contrast corrected** — `src/theme.cpp:75–76` selects `#282332` text for palette ordinal 3 while preserving the other foregrounds and all fills. The calculated contrast is now **5.11:1**, increased from 2.68:1, suitable for the normal 15px text.

## Compliance and code quality verdict

**Approved for the reviewed scope; no outstanding actionable findings.** The requested four themes and optional legacy Lab entry are present in the required order. Semantic document fields are untouched by `setThemeId`; switching is one checkpoint with a duplicate-ID no-op. Unknown IDs are rejected, missing legacy JSON IDs default to Lab, and explicit unknown JSON IDs are validated before mutation. Undo/redo restore theme state. Top-level branch ordinals are cached during rebuild, avoiding per-frame ancestor walks. Both render paths consume the same resolved shape/color data, and the text cache checks resolved foreground color. The inspector uses focusable Button delegates with accessible names and a current-theme indicator.

The new tests cover catalog order, representative appearance, reparented branch coloring, theme history/JSON, and rejected title drafts. GPU/software screenshots and export parity require runtime evidence, and the source-only review does not claim that those checks passed. No additional actionable correctness or performance defects were identified in this pass.

Follow-up verification was limited to these two fixes, as requested. Root reports the full macOS CTest run passes all three targets; native theme screenshots remain a separate root-task verification.
