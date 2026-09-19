# Automatic node font size

## Initial Prompt
Make node font sizes automatically decrease gradually with depth, while retaining custom font options.

## Plan
Use a shared depth scale with a readable minimum. Apply it consistently to measurement, rendering, rich text editing, calendar geometry and exports. Replace manual size entry with an automatic-size indicator. Verify formatting, hierarchy moves, persistence and interactions.

## Proposed Next Steps
Evaluate the visual scale on typical daily planners and large maps; verify on Linux and Windows.

## Implementation Summary
Implemented logical pixel sizes 20, 18, 17, 16, 15, then 14 at deeper levels. Family and other text formatting remain editable. Existing HTML size declarations are overridden for display without rewriting files on open. Depth-aware caching updates branch measurements after reparenting. Inline pasted content, calendars and export rendering use the same policy. macOS Release build succeeded; all nine CTest suites passed (132.86 seconds), including hierarchy movement, rich text paste, formatting persistence, calendar targeting and export checks. Inspected the rendered UI screenshot. Normal quit was cancelled by the running application; preserved its session and opened the rebuilt app separately.
