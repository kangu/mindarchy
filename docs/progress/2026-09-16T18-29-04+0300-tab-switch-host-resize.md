# Tab switch must not resize the host window

## Initial Prompt
Analyse the current Qt project, then implement work on features. After review, the first implementation was: fix a bug where switching between two document tabs resizes the host window, incorrectly triggered by child-tab window positioning code.

## Plan
1. Stop `MacApplication::activate()` from calling `QWindow::show()` on an already-visible host (`show()` maps to `showNormal()` and can change geometry).
2. Skip `WindowPlacement` constructor `resize()` / `setPosition()` when the window is already visible, so a child document’s saved rectangle cannot be applied to the shared host.
3. Pin each `DocumentWorkspace` to the host content item size so a hidden-then-shown tab cannot publish a different implicit size.
4. Add application and window-placement regressions; rebuild macOS; skip the full CTest suite for this pass (run it on every third feature change).

## Next Steps
Manual check: two tabs, user-resized window, switch both ways; maximized switch on macOS; detach should still offset a new window. Run the full CTest suite on the next-but-one feature change after this skip. Windows and Omarchy tab runtime remain untested on device.

## Implementation Summary
Tab activation no longer treats a child workspace as a window show. `activate()` only calls `showNormal()` when minimized and `setVisible(true)` when hidden; a visible host is raised and activated without `show()`. `WindowPlacement` still restores geometry for a hidden window at construction, but leaves an already-visible host alone. Each document workspace fills the host content item and reports that size as its implicit size.

`switchingTabsDoesNotResizeHost` resizes the host, opens a second tab, switches both directions, and on Cocoa checks maximized geometry. `visibleWindowKeepsGeometryWhenPlacementAttaches` asserts attaching placement to a visible window does not move it.

Built `application_test`, `windowplacement_test`, and `mindarchy` in `build-macos`. Focused CTest: macapplication, windowplacement, and application all passed (29.41 seconds). `git diff --check` passed. Full suite skipped per the two-then-one rule. Shortcuts unchanged.
