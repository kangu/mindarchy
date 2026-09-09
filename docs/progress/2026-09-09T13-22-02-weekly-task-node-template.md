# Initial Prompt
Introduce node templates added under the selected node. First template: choose a calendar week number and generate weekday nodes with an empty task under each weekday.

# Plan
Introduce a template catalog and atomic insertion API. Provide a themed calendar week picker, with ISO week numbers and month navigation. Create a dated weekly task branch with Monday–Friday task groups and empty task leaves. Preserve current children and integrate existing task progress, editing, persistence, and undo. Validate model and UI behavior.

# Proposed Next Steps
Restart the rebuilt macOS app, select a node, and choose Add node template from the header. Further templates can extend the catalog and insertion dispatch. No installer or remote deployment was requested.

# Implementation Summary
Added the Weekly task list template, accessible using the calendar icon beside Add child. The dialog starts at the current week and supports previous/next month, Today, and selecting ISO week numbers. Adding inserts 11 ordinary nodes beneath the selected parent: one dated week, five weekday task groups, and five empty task placeholders. The first placeholder receives editing focus. Week/day completion uses existing aggregate task progress. Insertion preserves existing children, unfolds the parent, validates date/node/depth limits before mutation, and is a single undo operation. No document format change was needed.

Built macOS successfully. Focused model tests cover year-crossing ISO weeks, invalid input, five weekday groups, task aggregation, save/reopen, and undo/redo. The UI test drives week selection, insertion, automatic focus, and typing. Native macOS flow passed and the calendar screenshot was inspected. All six CTest suites passed (50.79 seconds). git diff --check passed.
