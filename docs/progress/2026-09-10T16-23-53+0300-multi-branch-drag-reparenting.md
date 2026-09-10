## Initial Prompt
Fix drag-and-drop re-parenting in manual and automatic placement, including moving multiple selected branches and their descendants to a new parent.

## Plan
1. Trace drag selection and drop handling in both layouts.
2. Implement an atomic batch branch move with cycle validation and stable ordering.
3. Preserve manual subtree geometry and support group previews and free movement.
4. Verify engine and mouse-driven UI behavior, rebuild, and restart macOS.

## Next Steps
No required implementation steps remain. Manual testing can use the rebuilt macOS app. Installer packaging and deployment to other operating systems were not part of this task.

## Implementation Summary
- Both placement modes now support dropping entire branches onto a new parent.
- Dragging a selected node preserves multi-selection and moves all selected branch roots in tree order; selected descendants travel once with their ancestors.
- Batch changes are validated before application and undo as one operation. Self/descendant targets and invalid geometry are rejected.
- Manual re-parenting places branches beside their destination, preserves their relative visible geometry, and keeps unrelated nodes in place. Empty-canvas drops retain free movement, including mirrored branches.
- Added engine coverage for atomic undo/redo, nested selections, mixed parents, persistence, ordering, and mirrored group geometry. Added actual mouse-drag UI coverage across horizontal/vertical and manual/automatic layouts.
- Updated README with branch-drag behavior.
- Rebuilt macOS successfully. Full engine, canvas, and UI suites passed: 3/3 suites, zero failures (55.09 seconds).
- Gracefully quit and restarted the updated macOS app; restored document windows were present in the native Window menu.
