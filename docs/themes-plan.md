# First four themes implementation plan

Approved by user: implement theme switching and the first four themes in the installed MindNode Classic Themes panel. Scope: Beach Day, Holographic, Retro, Arcade. Observed on scratch Mind Map 3; geometry/fonts are portable approximations, not extracted proprietary assets.

- [x] T1: Add a theme catalog and document theme property, validation, undo/redo, JSON persistence and engine tests. Expose read-only theme metadata and node-resolved styles to canvas/QML. Keep existing Lab theme as backward-compatible default.
- [x] T2: Apply resolved fill/border/text/shape styles in GPU and software renderer, editor and canvas background; invalidate text textures on theme change. Add visual/behavior tests.
- [x] T3: Add a final Themes inspector tab with four original previews, keyboard-accessible Apply actions and current theme indicator. Guard drafts before switching.
- [x] T4: Review/test on macOS and Omarchy, capture each theme, launch updated app and update reference/PRD/results.

Ruling: repository root is not a Git repository; changes remain isolated to the existing qt-prototype subdirectory, with no forced Git initialization or commits. No gesture implementation is included; the preceding request asks only for its plan.

Validation: red UI applyTheme test reproduced missing functionality; red notes test reproduced draft loss. Final CTest on macOS: 3/3 suites pass. Omarchy qmake tests: 22 engine / 5 canvas / 15 UI passes, no failures. Native theme activation/draft/export checks pass on Cocoa and Wayland (5 including setup/cleanup on each). Four theme screenshots inspected. Review findings on notes loss and Retro contrast fixed and scoped review approved.

Implementation uses 15px portable default text plus preserved rich-text formatting; exact MindNode font metrics and manual per-node theme overrides are not reproduced. Pixel-perfect parity is not claimed. No native gesture implementation was added in this task.
