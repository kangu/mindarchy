# Initial Prompt
For the date view where you have the month selection, provide a subtle week number indicator somewhere on the line of every week.

# Plan
Add a muted ISO week label after Sunday for every month row. Reserve space before numeric totals and verify year boundaries and existing date interactions.

# Next Steps
Shared rendering will be included in the next builds for other platforms.

# Implementation Summary
Added small theme-colored W-prefixed ISO week labels at 55% text opacity. Month nodes gain a 32-unit gutter; sum columns move accordingly. Day hit areas and week-only views are unchanged. Shared painting includes these labels in exports.
macOS build passed. Targeted ISO year-boundary and calendar UI tests passed (6 results, zero failures). Inspected the rendered month screenshot showing W36–W40 beside the rows. Rebuilt and restarted macOS with recovery-aware quit.
