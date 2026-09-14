# Windows integrated header

## Initial Prompt
Hide the Windows title bar, merge the system controls into the header's right side, and drag the window by dragging the header.

## Plan Followed
Retain the native Windows resize frame without a caption. Place standard minimize/maximize/close buttons at the right of the QML header, reserve their width, and enable system dragging and double-click maximize on empty header space. Verify actual Windows interactions and screenshot rendering alongside both platform UI suites.

## Next Steps
The latest binary is open in the Windows VM for manual testing. Build the installer manually when ready; the existing installer predates this header change.

## Implementation Summary
- Windows has no separate title bar. Three 46-pixel-wide header buttons use Windows symbol glyphs, vertically centered with the toolbar. Minimize/maximize have a subtle hover background; Close turns red.
- Header dragging uses Qt's native system move operation. Double-click toggles maximize/restore. The native resize frame remains enabled; toolbar buttons receive their own clicks.
- Close uses the existing window-close path, preserving the native unsaved-document confirmation. Alt still reveals the menu bar.
- Avoided Qt's expanded-caption overlay: VM screenshots exposed hover compositing and toolbar occlusion in that approach. The final Windows flags omit both the caption and expanded-client-area hint, retaining the resize frame; macOS retains its existing flags.
- Added Windows header layout/frame checks to `tests/ui_test.cpp`. Actual Windows 11 VM mouse tests passed for dragging, edge resizing, double-click maximize, maximize/restore, minimize, and unsaved-close/discard.
- Evidence: `artifacts/windows-header-vm/native-header-verification.json`, `integrated-windows-header.png`, and `running-build.json`. The screenshot was captured inside Windows, including a hovered button.
- Final UI suites passed: Windows 84.08 seconds; macOS 42.82 seconds. The two native Windows header/menu cases also passed. `git diff --check` passed. No installer rebuilt.
- The temporary VM worker was stopped after verification; the updated application remains open for manual testing.
