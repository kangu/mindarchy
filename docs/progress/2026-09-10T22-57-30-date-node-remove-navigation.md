# Initial Prompt
For the date node type, make sure that once a date or week has been set, it cannot be modified with < > buttons on the UI.

# Plan
Remove previous/next calendar controls and their canvas hit areas, retaining deliberate date configuration and template selection. Verify month/week interactions and rebuild macOS.

# Next Steps
No additional steps required. Shared UI changes are available to other platforms on their next build.

# Implementation Summary
Removed previous/next buttons from date-node rendering and the sidebar, including obsolete geometry and pointer/click handling. Expanded the calendar heading into the freed space. Explicit date input, Today, view selection, and template selection remain available.
Updated tests to confirm clicks at former arrow positions leave the configured anchor unchanged in both month and week views, including a node with an image. Verified sidebar controls are absent and date entry editing still works.
macOS build succeeded. Targeted canvas and UI tests passed (6 test results, zero failures). Restarted the macOS app using its recovery-aware quit flow.
