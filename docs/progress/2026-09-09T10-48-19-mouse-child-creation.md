# Initial Prompt
Replicate MindNode Classic's hover plus handle: click creates a child, drag previews a branch and creates a child on release, with manual versus automatic placement.

# Plan
1. Inspect click and drag behavior in MindNode Classic scratch documents.
2. Add pointer creation to the engine with a single history checkpoint.
3. Render hover handles and live previews in software and GPU canvases; integrate cancellation and inline editing.
4. Test placement, editing, inheritance, persistence, and rendering; rebuild without packages.

# Proposed Next Steps
No required work remains. Scratch MindNode reference documents are retained, as recorded in the interaction reference.

# Implementation Summary
Inspected MindNode Classic using CUA and confirmed click-to-create inline editing, automatic placement after a dragged creation, and exact release placement in manual mode. Implemented a constant-size hover plus handle, reachable hover gap, live theme-colored ghost branch, click/drag creation, Escape/outside-canvas/lost-capture cancellation, blank inline editing, mirrored manual placement, and single-step undo/redo. Task inheritance and folded-parent expansion reuse the engine creation path. Added engine, canvas, and integrated UI tests for click/drag, cancellation, immediate typing, task inheritance, mirrored placement, and save/reopen. All six CTest suites passed; native Metal overlay rendering passed and was visually checked. Synthetic hover-input tests under Cocoa did not receive hover events in the background test window; interaction tests passed on Qt offscreen, and Metal rendering was verified separately. Rebuilt the development app only; package builds remain stopped.
