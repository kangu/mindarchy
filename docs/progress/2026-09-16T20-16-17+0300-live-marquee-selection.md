# Live marquee selection preview

## Initial Prompt
While dragging a selection rectangle, update live which nodes will be selected on release so the interface feels snappy.

## Plan
Compute intersecting nodes on each marquee move and draw them as selected immediately, without committing engine selection until mouse release.

## Next Steps
None. Shift/Command still extends the existing selection on release.

## Implementation Summary
Dragging on empty canvas still draws the marquee. Nodes that intersect it now get the selected outline immediately. Engine selection (inspector/outline) updates on release, same as before. Canvas test `marqueeHighlightsNodesBeforeRelease` passed. Rebuilt and restarted macOS. Full suite skipped. Shortcuts unchanged.
