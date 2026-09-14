# Initial Prompt
Delete a selected image with Delete, without deleting its node.

# Plan
Reproduce image selection deletion, prioritize images in the canvas keyboard handler, guard key repeat, update shortcut help, and verify both platforms.

# Next Steps
Click an image to show its handles, then press Delete or Backspace. Reopen existing Omarchy windows for the update.

# Implementation Summary
Delete and Backspace now remove the selected image only. Node selection still deletes the branch. Automatic key repeat is ignored to prevent a held key from deleting the node after its image disappears. Image removal remains undoable. Updated macOS and shared shortcut tables and README.

Validation: regression test failed before the fix (15 nodes became 10) and passed afterward. Tested both keys, preserved children, undo restoration, repeat protection, and ordinary branch deletion. All 28 canvas test results passed on macOS, plus the Cocoa image clipboard interaction test. The targeted regression passed on Omarchy. Rebuilt/restarted macOS and atomically updated Omarchy's executable without closing existing windows. No installer or commit.
