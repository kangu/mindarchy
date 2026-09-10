# Initial Prompt
Update the date node inspector with Border, Font and Text settings like regular nodes. Move month week numbers to the left of calendar dates and remove the W prefix.

# Plan
Expose shared appearance controls for date nodes, connect calendar typography and scaled geometry to existing rich-text settings, move week labels left, and verify styling and date interaction.

# Next Steps
The shared changes will be included in subsequent builds for other platforms.

# Implementation Summary
Date nodes now expose the shared appearance panel, including border, font and text controls. Fixed title-width controls remain hidden because the calendar determines its grid width. Calendar painting uses the node font family and formatting; font size scales its layout and hit targets proportionally. Font changes invalidate the calendar cache and are reflected in exports. Existing undo/persistence/reset machinery retains the settings.
Moved plain ISO week numbers into a muted column before Monday; shifted date cells and hit regions together while keeping totals separate.
macOS build and targeted tests passed: calendar year boundaries, scaled date styling and day targets, regular-node styling, date entries/hover/drag, image/calendar geometry, and uniform cell bounds (12 test results, zero failures). Inspected the rendered month with numeric totals. Restarted the macOS application through recovery-aware quit.
