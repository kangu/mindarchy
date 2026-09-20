# Keyboard-operable welcome panel

## Initial Prompt
Make the welcome panel fully operable with the keyboard.

## Plan
Define spatial grid navigation and complete focus traversal. Add activation, first/last jumps, escape, scrolling and picker focus restoration. Update platform shortcut help and verify representative keyboard-only flows offscreen.

## Proposed Next Steps
Confirm native file-picker behavior on each deployment platform during release validation.

## Implementation Summary
Added arrow-key navigation across the responsive grid, Tab/Shift+Tab traversal, Enter/Space activation, Home/End jumps and Escape to New Map. The first available recent map receives initial focus; empty history starts at New Map. Disabled missing files are skipped. Explicit focus policies, visible borders and scroll-follow behavior keep controls reachable at narrow sizes. File-picker cancellation reactivates the homepage and restores invoking focus. Component-owned timers prevent delayed focus callbacks after the homepage is destroyed. Updated macOS and QML shortcut help plus controls documentation. Targeted offscreen tests passed for navigation, file-picker open/cancel, focus restoration, narrow-screen scrolling, missing-file skipping, recent-map opening, empty-state creation and existing preview/camera behavior. macOS app build and diff whitespace checks passed; full UI/native suites were not run. Opened the rebuilt homepage separately to preserve existing sessions.
