# Keep double-click-zoomed windows when cycling

## Initial Prompt
On macOS, two windows zoomed to the screen edges by double-clicking the title snap back to their previous size when cycling with Cmd+`.

## Plan
Present the target window with AppKit `makeKeyAndOrderFront` and restore native zoom if Qt raise/activate toggles it. Treat `isZoomed` as maximized when saving placement so the restore-down rectangle is not overwritten.

## Next Steps
Manually: two windows, double-click each title to fill the screen, Cmd+` between them. They should stay zoomed.

## Implementation Summary
`presentMacWindow` records whether the window is zoomed, activates it, then re-applies zoom and the zoomed frame if Qt undid AppKit zoom. `MacApplication::activate` uses that on Cocoa. Placement capture now keeps the pre-zoom rectangle while `isZoomed`.

`cyclingWindowsKeepsNativeZoom` passed. macapplication, windowplacement, and application tests passed. Rebuilt and restarted macOS. Full suite skipped.
