# Trackpad pan gesture lock

## Initial Prompt

On Omarchy, two-finger panning must not switch to zoom during vertical movement.

## Plan

Inspect wheel classification, preserve pan intent for a complete gesture, add regression coverage, and apply the targeted fix to the connected Omarchy build.

## Next Steps

Verify real trackpad input manually in the updated Omarchy window. Automated tests reproduce Qt gesture events, not physical touchpad hardware.

## Implementation Summary

- Replaced per-event pixel-delta classification with a pan lock. Touchpad devices, pixel scrolls and phased scroll gestures identify panning. Angle-only updates retain both axes; momentum and modifier changes cannot turn an active pan into zoom.
- ScrollEnd releases the lock. Streams without phases release it after a 250 ms pause. Mouse wheel zoom remains available outside a pan gesture.
- Added regression coverage for angle-only updates, momentum, Control midway through panning, TouchPad device events without phases, and mouse zoom after the gesture.
- Local macOS build succeeded; canvas suite passed (1.83 seconds) and UI suite passed (47.10 seconds). No keyboard shortcuts changed.
- Connected to Omarchy and backed up its canvas implementation, header and test file with .before-pan-lock suffixes. Applied only this fix to the existing remote source, preserving its other feature versions.
- Built the app successfully on Omarchy 4.0.2 with Qt 6.11.2/GCC 16.2.1. The regression test passed offscreen. Initial Wayland testing identified that the default pointing device was a touchpad, invalidating the test’s assumed mouse. The test now constructs explicit mouse and touchpad devices.
- Launched updated PID 246936 as a new New mindmap window on workspace 2; Hyprland confirmed it mapped. The runtime log reports Wayland/OpenGL without QML errors. The prior running window was left untouched to preserve the user's work; use the new window for the fix.
- `git diff --check` passed. No installer packages were built.
- Corrected device-specific regression passed on Omarchy offscreen (284 ms) and native Wayland (297 ms), three test phases passed and zero failures each. Local canvas suite also passed again (1.95 seconds). A temporary test-only compile flag override initially omitted Qt feature defines; restoring those defines fixed the test build without changing the application.
