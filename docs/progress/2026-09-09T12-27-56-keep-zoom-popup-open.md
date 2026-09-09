# Initial Prompt
Keep the zoom popup open after Zoom In or Zoom Out, on macOS and Omarchy.

# Plan
Add repeated-click regression coverage, replace automatic menu dismissal with explicit popup behavior, build both platforms and test native interactions.

# Proposed Next Steps
Restart existing app windows when convenient to load the updated build.

# Implementation Summary
Changed the zoom dropdown from an automatically closing Menu to a styled Popup with explicit dismissal. Zoom In and Zoom Out remain open for repeated clicks. Actual Size and Fit Map close after applying; Escape and outside-click dismissal remain available, with arrow-key navigation between items. Regression test initially failed on the old close-after-click behavior, then passed for repeated zoom-in/out clicks, reset, fit and Escape on native macOS Cocoa and Omarchy Wayland. Rebuilt macOS and deployed the updated Omarchy binary; existing document windows were not restarted. No installer packages built.
