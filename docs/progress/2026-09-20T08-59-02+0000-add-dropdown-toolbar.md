# Add dropdown toolbar

## Initial Prompt
Condense the toolbar's three separate Add actions into an "Add ▾" dropdown placed next to the File dropdown, with child/template/sibling items and the existing menu behavior. Original ask: "Merge the three Add toolbar actions into an Add dropdown next to File".

## Plan
Followed `docs/superpowers/plans/2026-09-20-add-dropdown-toolbar.md` (see its "Ratified deviation" section): move the Add-child / Add-node-template / Add-sibling actions into a new "Add ▾" dropdown in the left documentActions group, reusing the File menu shell, with enabled-state gating per item and Escape / click-outside / keyboard handling.

## Implementation Summary
Replaced the three separate Add buttons in `qml/DocumentWorkspace.qml` with a single "Add ▾" dropdown immediately right of "File ▾", showing Add child, Add node template, and Add sibling with icons. The template item is enabled with a single node selected; child/sibling stay always actionable, matching the old toolbar. One ratification: the template item calls `nodeTemplateDialog.open()` directly instead of a toggle, because the menu shell closes before the dialog can receive a second toggle and the toggle branch was unreachable (and flapped under Qt 6.11 QTRY double-evaluation in tests). Undo/Redo/Fold remain in the center group. Two targeted commits fixed follow-ups (4a04ed5, 29c6a46 → cd6b6fe). Full verification passed: `dev.py ui` (65 passed, 0 failed, 3 skipped), `dev.py check` (engine, canvas, preview, manualplacement all pass), and `dev.py build` completed cleanly. One `ui` run showed a single transient test failure out of 64 that did not reproduce on two consecutive full runs, so it was treated as flaky; the failing case was not identified because the failure output only appears with `--output-on-failure`, which the passing reruns did not trigger. No Mindarchy instance was running at restart time (only unrelated Nuxt dev processes), so no restart was needed and no visual smoke check on a live window was performed.

## Proposed Next Steps
Run the Step 4 visual smoke check on a live window when the app is next opened: confirm Add ▾ placement next to File ▾, menu items and enabled states, Escape / click-outside / keyboard navigation, and that Undo/Redo/Fold remain in the center group. If the flaky ui test reproduces, identify and stabilize it with `ctest --output-on-failure`.

## Follow-up: visual smoke check completed (2026-09-20)

The freshly built app was launched and the on-window smoke check from Next Steps was
performed by the user with the controller driving screenshots: "Add ▾" sits next to
"File ▾" in the left group, the menu lists Add child / Add node template / Add sibling
with icons, Escape and click-outside close it, Undo/Redo/Fold remain in the center group,
and the toolbar renders without overlap. All checks passed. No app restart was needed
because no prior instance was running.
