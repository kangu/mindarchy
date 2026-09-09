# Initial Prompt
Allow selecting a different week by clicking it in the node template calendar dialog.

# Plan
Make the entire week row a selectable control, preserving its highlight and week-number action. Test clicks on different date columns and verify that selection changes without inserting until Add template is clicked.

# Proposed Next Steps
Restart the rebuilt macOS app to load the updated dialog.

# Implementation Summary
Each calendar week is now one full-width button. Clicking any day, week number, or space within its row changes the selected week and date-range label. The row retains hover/selection styling and exposes its selected state to accessibility. Updated the helper text and controls documentation. Rebuilt macOS; both template UI tests passed offscreen, and real date-column mouse clicks passed on native macOS. git diff --check passed.
