# Branch clipboard and Focus mode

## Initial Prompt

Let's implement Branch copy/paste and Focus mode.

## Plan

1. Implement validated subtree clipboard interchange with fresh IDs, preserved content and appearance, and atomic undo.
2. Add transient branch focus, dim unrelated content, and restore the original viewport on exit.
3. Expose both features in the header and keyboard shortcuts; update Help and documentation.
4. Verify engine, canvas, and macOS UI behavior, then restart the latest app.

## Next Steps

Manual testing is available in the running macOS build. No installer, Windows deployment, or Omarchy deployment was performed.

## Implementation Summary

- Added header branch clipboard menu and canvas Cmd+C / Cmd+V shortcuts. Complete selected subtrees copy between documents, retaining rich titles, notes, dates, tasks, styles, folding, and internal relationships. Overlapping selections are deduplicated. Pasting creates fresh IDs beneath the selected node and is one undo action.
- Clipboard validation rejects malformed or oversized input before changing the document. Indented plain text and Markdown lists can be pasted as a hierarchy; ordinary text editing retains normal copy/paste behavior.
- Relationships to nodes outside the copied subtree are omitted. Manual offsets are reset for placement in the destination map. Copied inherited appearance becomes explicit style to preserve its look across document themes.
- Added header Focus toggle and Cmd+Shift+F. The selected subtree and ancestor path stay visible while unrelated nodes and connections dim. A breadcrumb supports ancestor navigation; Escape exits and restores the original pan and zoom. Focus does not change folding or dirty the document.
- Saved viewport and recovery snapshots retain the view from before Focus. Search or keyboard navigation outside the branch exits Focus; deleting its root also exits safely.
- Updated native macOS Help and the shared Windows shortcut table, plus README usage documentation.
- Rebuilt macOS successfully. Full CTest passed all six suites (macwindow, engine, canvas, ui, windowplacement, preview), with zero failures in 55.81 seconds. Dedicated Cocoa UI integration passed with both the default and software renderers. Tests cover cross-document clipboard, preserved task/calendar/style data, internal relationships, undo/redo, malformed input, normal editor paste, focus dimming, and viewport restoration.
- Visually inspected screenshots at `artifacts/branch-focus/focus-macos.png` and `artifacts/branch-focus/focus-software-macos.png`.
- Gracefully quit and reopened the latest macOS app; the existing edited document was recovered and both new header controls are present.
