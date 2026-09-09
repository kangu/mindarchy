# Initial Prompt
Stop changing zoom when the window is resized.

# Plan
1. Trace every resize and zoom path.
2. Verify current behavior with a window-level regression test.
3. Rebuild macOS and record results.

# Proposed Next Steps
Use the rebuilt development application.

# Implementation Summary
Confirmed the resize-triggered fit timer was already removed by the viewport persistence change. Canvas geometry changes retain zoom and world-space center. Added a regression test exercising multiple window sizes and both side-panel toggles, with time for delayed callbacks. The test passed; macOS application rebuilt successfully and diff whitespace validation passed. No additional production behavior change was needed.
