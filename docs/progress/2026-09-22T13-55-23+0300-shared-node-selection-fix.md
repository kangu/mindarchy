# Shared node selection fix

## Initial Prompt
I can see a bug when a map is shared and i try to add a new node, i always get the first child node selected instead of the one i just added. only happens when sharing is on. investigate and fix

## Plan
1. Trace node creation, remote state projection and canvas editor identity.
2. Reproduce renumbering with an open new-node editor.
3. Preserve local IDs during remote application and validate selection/typing.
4. Run fast checks and rebuild.

## Next Steps
Commit/push the client changes before rebuilding Omarchy. Save work and reopen the rebuilt macOS app; the normal restart was cancelled.

## Implementation Summary
Fixed shared-map node selection/editor jumps caused by canonical WebSocket projections renumbering numeric node IDs. Remote application now maps stable sync identities back to existing local IDs, allocates fresh IDs for new remote nodes, remaps topology/connections and rebases undo/redo using the same identity mapping. Canonically identical acknowledgements no longer cause numeric-ID-only state replacement. Added a canvas regression that reproduced the failure before the fix and verifies selection, editing target and typing isolation for both acknowledgements and actual peer changes. All 12 fast suites passed, macOS build passed, and git diff --check passed. Normal restart was cancelled by the app (-128); the running session was preserved. No Go/server changes.
