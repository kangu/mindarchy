# Add dropdown toolbar redesign — 20 September 2026

Target: `/Users/radu/Projects/mindmap-blue/qt-prototype` (Mindarchy desktop Qt/QML app).

## Goal

Condense the three toolbar Add actions (Add child, Add node template, Add sibling) into a
single "Add" dropdown styled exactly like the existing File dropdown, placed immediately
after the File button in the left `documentActions` toolbar group.

## Current state

In `qml/DocumentWorkspace.qml` the toolbar center group (`editingActions`) contains:
Undo, Redo, a separator, Add child (`controller.addChild()`), Add node template
(`nodeTemplatesButton`, opens `NodeTemplateDialog`), Add sibling
(`controller.addSibling()`), and Fold/Expand branch (`controller.toggleFold()`).

The File button in `documentActions` is the model for the new dropdown: a `ToolbarButton`
with "File ▾" text, a `Menu` (width 240, padding 6, radius-10 background
`#172129`, border `#34434c`), `y: button.height + 8`, checked state bound to
`menu.visible`, click toggles, `Keys.onDownPressed`/`Keys.onReturnPressed` open it,
`focusForceActivate` of the first item on open, `CloseOnEscape | CloseOnPressOutside`.

## Design

1. New `ToolbarButton` with `objectName: "addMenuButton"`, `Accessible.name: "Add menu"`,
   icon/plus glyph and text "Add ▾", placed in `documentActions` directly after the
   `fileButton`.
2. Its `Menu { id: addMenu; objectName: "addActionsMenu" }` replicates `fileActionsMenu`
   styling and behavior exactly (same width, padding, background, keyboard handling).
3. Menu items, preserving today's trigger guards:
   - "Add child" (`corner-down-right` icon):
     `{ addMenu.close(); if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addChild() }`
   - "Add node template" (`calendar-week` icon), enabled only when
     `controller.selection.length === 1`, carrying `objectName: "nodeTemplatesButton"`:
     `{ addMenu.close(); if (!window.commitEditor("")) return;
        nodeTemplateDialog.visible ? nodeTemplateDialog.close() : nodeTemplateDialog.open() }`
     The single `NodeTemplateDialog` instance keeps its `id: nodeTemplateDialog` and moves
     from the old toolbar button into the new Add button so the id stays resolvable.
   - "Add sibling" (`list-plus` icon):
     `{ addMenu.close(); if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addSibling() }`
4. The Add button's `enabled` state becomes true unless every item is disabled
   (item-wise bindings preserved; the template item keeps the single-selection gate).
5. The center `editingActions` group now contains only Undo, Redo, separator and
   Fold/Expand branch. Fold logic, text toggle, and bindings are unchanged.
6. Native macOS/Windows/Linux menu bars (`desktopMenuSet`) are unchanged. The Add
   dropdown exists only in the toolbar, matching File's current toolbar scope.

## Tests

`tests/ui_test.cpp` currently finds `nodeTemplatesButton` at the toolbar level around
lines 360, 404 and 520. After the change the test must:
- find `addMenuButton`, simulate its `clicked()` to open the menu, then find the
  `nodeTemplatesButton` menu item and trigger it, before finding `nodeTemplateDialog`.
- the line-520 presence loop of buttons updates from `nodeTemplatesButton` to
  `addMenuButton` (menu-level items are not always constructed until the menu opens, so
  presence is asserted on the dropdown button).

Controller behavior assertions around child/sibling/template insertion remain valid because
the underlying controller calls are unchanged.

## Non-goals

- No change to existing keyboard shortcuts or controller calls apart from rebinding their
  triggers through the menu items
- No new controller/QML modules; all changes stay in `qml/DocumentWorkspace.qml` and
  `tests/ui_test.cpp`
- No file-menu additions
