# Windows Alt+F / Alt+T vs menu bar

## Initial Prompt
The Windows 0.1.4 installer build failed ui tests: type-to-replace task toggle and fold. application_test exited 0xc0000138.

## Plan
Stop showing the Windows menu on Alt-down so Alt+letter can reach the canvas. Block File-menu mnemonics for Alt+F/T while the bar is hidden. Run application_test offscreen on Windows with the font directory.

## Next Steps
Robocopy the tree into the Windows build folder and rerun `release-windows.ps1 -Version 0.1.4`.

## Implementation Summary
`WindowMenuBar` now toggles the menu on a bare Alt release. Alt+F still folds and Alt+T still toggles task when the menu is hidden. CMake runs `application` offscreen on non-Apple hosts. Shortcuts table unchanged.
