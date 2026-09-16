# Node template toolbar popover

## Initial Prompt
Transform adding a node template so the toolbar button opens a small menu popover with Weekly tasks and Meeting notes (buttons and icons). Configuration such as week selection should appear as a popover further down in the same area, not a centered dialog.

## Plan
1. Replace the modal centered `Dialog` with a `Popup` anchored under the toolbar button.
2. Show the two templates as icon+label actions; Meeting Notes inserts immediately.
3. Expand the same popover downward with the week calendar when Weekly task list is chosen.
4. Close the popover before inserting so the new node receives editing focus.
5. Update UI tests; skip the full CTest suite (second of two skipped feature changes).

## Next Steps
Manual: click the calendar toolbar button, pick Meeting Notes, then Weekly task list and confirm a week. Full CTest on the next feature change.

## Implementation Summary
The Add node template control opens a compact popover under the toolbar with Weekly task list (`calendar-week`) and Meeting Notes (`notebook-pen`). Meeting Notes inserts on click. Weekly task list expands the popover downward with month navigation, week rows, and Add template. Insertion still requires one selected parent, remains one undo, and starts editing as before.

UI tests `weeklyTemplatePickerAddsBranch` and `meetingTemplateStartsIndividualNoteEditing` passed offscreen (1.49s). Built macOS app. `git diff --check` passed. Full suite skipped. Shortcuts unchanged.
