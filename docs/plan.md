# Qt Mindmap Lab prototype

Goal: test the proposed Qt Quick + C++ stack on Omarchy (SSH omarchy-host), with portable sources for macOS and Windows.
Approved scope: sample prototype following the stack recommendation and observed MindNode Classic interactions. No replacement of existing application, no cloud sync or MindNode binary format parity promised.

Architecture: C++ document/layout/controller; Qt Quick custom GPU scene graph canvas with cached inactive text and one editing overlay; QML inspector and outline. Qt Core/Gui/Quick/QuickControls2/Test, C++20. qmake build supported because remote has Qt/qmake/make already; also supply CMake build.

Implementation tasks:
- [x] Engine: indexed ordered tree, horizontal/vertical/compact layout, folding, bounded snapshot undo/redo, cycle-safe reparenting, manual placement, configurable spacing and branch type, selection navigation, deterministic demo/1k/10k fixtures, measured layout time. Unit tests for hierarchy, layout, history and persistence.
- [x] UI: inspector, outline, scene graph geometry, cached labels, active title editor, keyboard modes, pan/zoom/fit, select/multiselect, drag preview/reorder/reparent/manual moves, tasks/notes/relationships, local JSON save/open, PNG export and telemetry.
- [x] Integration: build on remote with qmake, run engine and GUI tests, launch under Wayland, capture application screenshot, benchmark generated maps and record limits.
- [x] Review: inspect correctness and performance; fix findings; README with exact launch steps, controls, results and unverified features.

Acceptance: app launches on native Wayland; all three layouts preserve hierarchy; navigation follows layout; Return finishes editing then creates sibling (root child), Tab creates child, Ctrl/Command-Return edits; folding and drag changes undo; invalid reparent and invalid imports cannot corrupt document; zoom maintains cursor anchor; large fixture navigation remains usable; actual measured times are labeled and not equated with end-to-end latency.

Prototype boundaries: initial layout may recompute entire visible tree; report this honestly. Cached raster labels are acceptable for prototype, not final text renderer. SQLite, production import/export formats, collaboration and OS accessibility parity remain future work. Save/open uses versioned JSON with atomic replacement.
