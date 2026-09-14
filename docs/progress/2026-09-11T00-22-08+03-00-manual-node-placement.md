# Initial Prompt
Place newly created manual-layout nodes in the next logical free position based on the parent and complete sibling subtrees. Preserve existing manual positions and explicit drag-to-create destinations.

# Plan
Inspect insertion and manual offset geometry, add a shared placement helper, rebase offsets after insertion, and validate in a separate build without disrupting the main conversation's running app.

# Next Steps
The main application's next build will include this shared-source change. A separate macOS build is available at build-side-manual-placement/mindarchy.app; the running app was deliberately not restarted in this side conversation.

# Implementation Summary
Added manualplacement.h to infer branch direction, sibling alignment, flow and spacing. New children follow the full sibling subtree band; sibling insertion uses an existing clear gap when available. Collision checks include unrelated visible nodes. Placement covers horizontal, mirrored-left, vertical, ascending and unfolded branches. Explicit pointer destinations override automatic placement.
Insertion now rebases manual offsets after topology changes so existing visible node centers do not move. New positions remain undoable and persist through save/reopen.
Built the macOS app in build-side-manual-placement. All 8 dedicated placement test results passed, plus 5 existing engine regression results covering pointer placement, mirrored branches and new-document insertion. git diff --check passed. Initial temporary-directory builds encountered a Qt-generated relative-include issue on macOS; using an isolated build directory inside the repository resolved it without source workarounds.
