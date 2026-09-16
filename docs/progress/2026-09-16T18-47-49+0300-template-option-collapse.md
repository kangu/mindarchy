# Template options collapse into configuration

## Initial Prompt
When clicking a template that needs further configuration, fade out and collapse the other items, then open the configuration directly below the selected item.

## Plan
Keep each template and its configuration in one row. Unused rows fade and collapse. The week picker expands under Weekly task list. Update UI tests to wait for the expand animation.

## Next Steps
None for this interaction. Meeting Notes still inserts immediately.

## Implementation Summary
Choosing Weekly task list fades and collapses Meeting Notes, then opens the week calendar immediately under the selected row. Template UI tests passed offscreen. Rebuilt and restarted the macOS app. Full suite skipped. Shortcuts unchanged.
