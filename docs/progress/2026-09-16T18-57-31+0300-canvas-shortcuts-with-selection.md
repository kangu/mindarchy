# Canvas shortcuts that keep zoom keys with a selection

## Initial Prompt
When a node is selected, typing normally overwrites its title. Special keys such as + and − used for zooming should still zoom. Put those keys in a dedicated shortcut list that applies even with a node selected, so more can be added later.

## Plan
1. Add `src/canvasshortcuts.h` as the registry for bare canvas keys that skip type-to-replace.
2. Handle those keys in `MindCanvas::keyPressEvent` before starting title replacement.
3. Update Help tables, README, and tests.

## Next Steps
Add further reserved keys to `CanvasShortcuts::commands` when needed. Fit-map `0` still requires no selection so numeric titles can start with 0.

## Implementation Summary
`CanvasShortcuts` currently maps `+` / `=` to zoom in and `−` to zoom out with no modifiers. Those keys zoom whether or not a node is selected and do not start title editing. Other printable keys still replace the selected title. Help (macOS and Windows/Linux) and the README match that split; Fit map `0` remains a no-selection shortcut.

Canvas and UI tests cover zoom with a selected node and unchanged title. Full CTest: 9/9 passed (128.16 seconds). Rebuilt and restarted the macOS app.
