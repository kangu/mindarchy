# Initial Prompt
Support document tabs alongside separate windows, using native-like macOS behavior and a matching design on other systems. Add the standard Window menu tab actions. Cmd/Ctrl+T creates a tab; Cmd/Ctrl+N remains a new window. Ctrl+Tab and Ctrl+Shift+Tab cycle tabs. Keep Omarchy fully themed.

# Plan
Extend the existing single-process document manager to all interactive desktop platforms. Use AppKit tab groups on macOS and a scrollable shared tab strip on other platforms. Add creation/cycling/detach/merge actions, preserve per-document state and recovery groups, integrate live theme colors, and test desktop behavior.

# Next Steps
Use Cmd/Ctrl+T for a new tab and Cmd/Ctrl+N for a separate window. Fully quit older Omarchy/Windows instances before running the updated executable. Windows runtime validation remains outstanding; no Windows VM was used for this task.

# Implementation Summary
Added native macOS tabs with AppKit plus/close buttons, native tab dragging, forward/backward cycling, detach, merge, and Show Tab Bar actions. Reserved native tab-bar space above the app toolbar to avoid overlap. Cmd+T and the native plus button create tabs; Cmd+N remains a separate window. Physical Control+Tab and Control+Shift+Tab cycle on macOS.

Extended the application manager to Linux and Windows. The shared strip has titles, edited indicators, close buttons, a plus button, and horizontal overflow scrolling. Its colors, separators, hover states, and shortcut-help window use live ShellTheme colors. On these platforms each document retains its own Qt Quick window and only the selected document in each group is shown. Ctrl+T, Ctrl+N, Ctrl+Tab, and Ctrl+Shift+Tab are wired to the shared manager; the Window menu exposes previous/next, detach, and merge. Merge disables when there are no other groups to merge.

Each document retains its engine, canvas, undo stack, file path, viewport, and recovery snapshot. Closing removes that document; quitting preserves tab groups in session/tabs.ini outside user files. Native tab order and active tab are recorded. Updated both shortcut tables and README. Added native tab/recovery tests and a portable application integration target for Linux/Windows.

Validation: all seven macOS suites passed (73.88 seconds); subsequent native application/window/full UI regression checks passed (66.22 seconds), and the final menu/help check passed. In the running macOS app, verified Cmd+T, native plus, physical Control+Tab, Control+Shift+Tab, Cmd+N, and closing only disposable test tabs/windows. Inspected the corrected native tab spacing. Real Omarchy Wayland integration tests passed for shortcuts, visibility, detach, merge, close, recovery, and live theme changes including shortcut help. Inspected a themed Omarchy screenshot. Atomically updated Omarchy's binary and restarted macOS. Windows shares the implementation and build/test target but was not compiled or runtime-tested here. No installer or commit created.
