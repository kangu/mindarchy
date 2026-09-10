# Omarchy application menu

## Initial Prompt

Provide a hamburger icon in the top-right corner on Omarchy, opening the same menu structure as the desktop application on macOS and Windows.

## Plan

Share File and Help menu definitions with Windows, add the macOS-style Window commands and document list, expose them through a Linux-only header menu, verify actions and focus restoration, then build and launch on Omarchy.

## Next Steps

Manual testing in the updated Omarchy window. Hyprland controls placement and activation policy; Center is disabled on Linux. Native macOS system-provided menu additions remain OS-managed.

## Implementation Summary

- Added a theme-tinted hamburger after the inspector toggle on Linux. It opens File, Window, and Help submenus and dismisses with Escape/outside click.
- Extracted DesktopMenus.qml so Windows and Linux use the same File, Window, and Help definitions. Windows retains its Alt menu-bar entry point; macOS retains its native menu.
- File commands call existing New, Open, Save, Close, and Quit flows, retaining unsaved-change handling. Help opens the shared Keyboard Shortcuts window and restores document focus on closing.
- Window includes minimize, maximize/restore, full screen, next/previous, Bring All to Front, and live documents. Engine delegates window enumeration and activation to DocumentSession, matching native macOS's registry. Stale activation targets are rejected. Center remains present but disabled on Linux because placement is compositor-controlled.
- Updated shortcut reference wording to show the Linux header-menu entry point and documented behavior in README. No new shortcut combinations introduced.
- Added engine navigation and shared-menu UI tests. The UI test verifies submenu counts, the Linux hamburger, Escape dismissal, New dispatch, Help opening, and focus restoration. The full local engine suite and UI suite passed, with a final focused menu test passing after the Windows loader initialization adjustment.
- Backed up the Omarchy source to /home/user/mindmap-before-desktop-menu-20260909.tar.gz before synchronizing current sources and building its updated app and test harness.
- Pinned the Linux hamburger outside the horizontally scrolling toolbar, so narrow windows retain menu access. Its tooltip is suppressed while the menu is open.
- Omarchy build succeeded. Menu regression passed offscreen (4.02 seconds) and on native Wayland (8.44 seconds), including menu opening/dismissal, command dispatch and shortcut-window focus. Visually inspected artifacts/desktop-menu/wayland.png.
- Started the final updated Omarchy build as a fresh New mindmap window, PID 255110. Startup reports Wayland/OpenGL and no QML errors. Existing document windows were preserved. No installer packages built; Windows was not runtime-tested.
- `git diff --check` passed.
