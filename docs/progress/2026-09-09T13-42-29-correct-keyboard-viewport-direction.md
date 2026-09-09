# Initial Prompt
Command-arrow must move the viewport in the arrow direction, regardless of natural-scroll settings: Left goes left and Up goes up.

# Plan
Reverse keyboard pan offsets and verify world coordinates at the viewport center, rather than artwork translation. Keep zoom, selection, repeat, and editing behavior intact. Update shortcut documentation.

# Proposed Next Steps
Restart the rebuilt macOS app to load the correction.

# Implementation Summary
Corrected all four Command/Ctrl-arrow offsets so viewport travel follows the pressed arrow. The keyboard handler remains independent of wheel-event inversion and macOS natural scrolling. Updated the regression test to measure world-space viewport-center movement for each direction and repeated keypress, alongside unchanged zoom/selection/document revision and text-editing exclusion. Updated the native keyboard shortcut reference and controls documentation. macOS rebuilt successfully and the focused native test passed.
