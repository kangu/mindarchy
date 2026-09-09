# Initial Prompt
Make the percentage a compact dropdown containing all zoom/fit/actual-size actions; run and debug only on Omarchy.

# Plan
Refactor QML controls, update action/layout tests, build and test remotely in offscreen and native Wayland modes, inspect rendering and launch a fresh updated window.

# Proposed Next Steps
Manual testing in the new Mindarchy window on Omarchy workspace 2.

# Implementation Summary
Replaced the header zoom row with one 76-pixel percentage dropdown. Its menu contains Zoom In, Zoom Out, Actual Size (100%) and Fit Map with matching icons and shell colors; opening it does not alter zoom. Suppressed the button tooltip while the menu is open. Updated interaction tests for compact sizing, menu opening, every action, dismissal and toolbar spacing. Built and debugged only on Omarchy: focused offscreen tests passed, and the actual Wayland menu actions passed with a screenshot visually checked. Deployed the binary and launched a separate New mindmap window, PID 126971, visible on workspace 2, preserving the previous document window. No macOS build/run and no installer packages.
