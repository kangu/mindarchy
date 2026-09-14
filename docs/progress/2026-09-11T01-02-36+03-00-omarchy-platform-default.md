# Omarchy platform default

## Initial Prompt
Detect Omarchy on first run and default to its theme, with Porcelain second.

## Plan
Detect installed Omarchy from its installation or theme-state markers, resolve the blank-document default at runtime, and reorder the theme catalog and default badge consistently. Preserve saved document themes.

## Next Steps
Verify on the next Omarchy build. Reopen the rebuilt macOS app if needed; automatic restart remains unavailable through the app-control connector.

## Implementation Summary
Linux checks OMARCHY_PATH, /usr/share/omarchy, ~/.local/share/omarchy, and XDG state/config theme locations. Detected Omarchy systems create new documents with the Omarchy theme and show Omarchy then Porcelain. Other systems retain Porcelain then Omarchy. Existing document loading is unchanged, and fresh documents remain unedited. No installation hook or migration is required.

macOS build succeeded. All 64 engine tests passed, including filesystem detection, default ordering and badges. The Cocoa theme-picker test passed on rerun after an initial focus failure during setup. Runtime detection on a real Linux host has not been exercised in this task. No package or deployment was made.
