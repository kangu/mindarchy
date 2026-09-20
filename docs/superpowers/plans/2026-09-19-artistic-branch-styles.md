# Artistic branch styles implementation plan

**Goal:** Add Botanical graphite, Living oak, Sumi branch, Silver birch and Elven filigree as persisted, undoable map connector styles with automatic light/dark canvas palettes.

**Design:** Share deterministic tapered stroke geometry between QPainter previews/software and GPU triangle rendering. Seed detail by node identity, keep legacy lines/cross-links unchanged, and simplify detail at low zoom. Extend the existing map inspector with a labeled style picker. Preserve unrelated local window-placement edits.

- [x] Add style round-trip/undo tests and deterministic geometry, palette and layout tests; observe failure before implementation.
- [x] Extend the shared drawing layer with style registry, background-aware palettes, tapered silhouettes, grain, bark, ink and filigree geometry. Reuse it in canvas GPU/software and preview rendering.
- [x] Expose all seven styles in the map inspector; document persistence and theme adaptation.
- [x] Build and run focused then full tests. Render light/dark examples for all five styles and inspect results. Restart the latest macOS build and record progress.
