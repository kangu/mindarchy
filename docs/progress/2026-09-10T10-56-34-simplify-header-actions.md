# Simplify header actions

## Initial Prompt

Remove Branch copy / paste, Focus, and Connect selected nodes from the top toolbar, keeping their functionality accessible through keyboard shortcuts.

## Plan

Remove the controls, preserve existing copy/paste and Focus shortcuts, add a Connect shortcut, update Help and verification, and rebuild macOS and Omarchy.

## Next Steps

Manual testing in the restarted macOS app or the newest Omarchy New mindmap window. Previous Omarchy windows were preserved and still run their earlier binaries.

## Implementation Summary

- Removed all three toolbar buttons and the branch clipboard popup. Focus breadcrumbs and Exit Focus remain available during Focus mode.
- Retained Cmd/Ctrl+C and V for branch clipboard operations and Cmd/Ctrl+Shift+F for Focus. Added Cmd/Ctrl+L to connect exactly two selected nodes. Normal text editing retains its keyboard behavior.
- Updated native macOS and shared Linux/Windows keyboard-shortcut tables and README. Tests check removed controls, keyboard clipboard/Focus, connecting two nodes, and undoing the connection.
- macOS build passed; native Cocoa shortcut test passed, and macwindow/UI suites passed (48.69 seconds). Restarted the app and verified the three controls are absent in the native accessibility tree.
- After the user's Tailscale authentication, updated and built Omarchy. Shortcut tests passed offscreen (723 ms) and on Wayland (871 ms). Launched updated PID 645288 as a fresh New mindmap window, preserving existing windows; startup log is clean.
- `git diff --check` passed. No installer package built.
