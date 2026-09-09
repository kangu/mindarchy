# Initial Prompt
For every opened file, remember viewport location and zoom outside the file, and restore them on reopen.

# Plan
1. Inspect canvas transforms and startup/open/save paths.
2. Add local per-file settings with validation, debounced writes, and shutdown flushing.
3. Integrate restoration and preserve the view during resizing.
4. Test reopen, Save As, resized windows, invalid settings, and unchanged document bytes; rebuild macOS.

# Proposed Next Steps
Optional: verify interactively on Omarchy. Renaming or moving a document creates a new path identity.

# Implementation Summary
Implemented machine-local per-file viewport persistence using Qt user settings under Mindarchy/Mindarchy. Canonical absolute file paths are hashed as keys; zoom and world-space viewport center are stored with a 300 ms debounce and flushed when switching documents, saving, and shutting down. Save As carries the current view to the new file. Reopening restores the view; absent or invalid settings fall back to fit. Window resizing preserves the world-space center instead of fitting automatically. Document JSON remains unchanged. macOS build succeeded, all six CTest suites passed, and the focused persistence test passed again after final cleanup. Omarchy uses the same cross-platform settings implementation but was not run remotely.
