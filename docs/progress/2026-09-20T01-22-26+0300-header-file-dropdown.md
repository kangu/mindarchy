# Consolidated header file menu

## Initial Prompt
Merge the four top-left header file-action icons into a dropdown-like file-handling control with a nice icon or illustration.

## Plan
Replace the icon row with a compact File dropdown using the existing header palette. Preserve New/Open/Save/PNG export behavior and shortcuts. Verify commands, keyboard dismissal, header geometry and the rendered design.

## Proposed Next Steps
Review the compact menu during everyday use; retain native platform checks for release validation.

## Implementation Summary
Added a document-stack vector icon and File chevron control in place of four separate buttons. The rounded menu groups New/Open and Save/Export with matching action icons. Preserved existing file shortcuts, save handling and dialogs; New and Export commit pending title edits. Corrected the inherited square-button height for the wider control. Five targeted offscreen UI cases passed for menu operation/export, save, new-document behavior, responsive toolbar layout, focus feedback and title layout. Inspected the final screenshot. macOS build and diff checks passed; full native/UI suites were skipped. Opened the rebuilt application separately, preserving existing sessions.
