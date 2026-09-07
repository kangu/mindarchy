# Mindmap Lab controls

The prototype stores local versioned JSON documents. Large test fixtures are available through the `--nodes 1000` or `--nodes 10000` launch options. PNG exports the current canvas viewport, so use Fit map before exporting the whole visible map.

## Canvas

- Click a node to select it; Shift-click extends the selection.
- Double-click or Ctrl/Command-Return edits the selected title.
- Tab creates a child; Return creates a sibling (a child when the root is selected).
- Arrow keys navigate the tree; Shift-arrow extends selection.
- Delete/Backspace deletes the selected branch. Undo restores it.
- Drag a node onto another node to reparent its branch. In Manual placement mode, drag to position a branch freely.
- Drag empty canvas to select a rectangle of nodes. Hold Space while dragging, or drag with the middle/right mouse button, to pan. Trackpad scrolling pans; the mouse wheel or Ctrl-scroll zooms around the pointer. The zoom buttons and Fit map are available at the bottom of the canvas.
- Use the Actual size icon to reset zoom to 100%. Hover an icon to see its tooltip.
- Shift-select two nodes and press Connect to add a relationship line.
- Fold/Expand hides or reveals descendants without deleting them.

## Editing

One rich-text editor opens over the active title. Return commits; press Return again on the canvas to create a sibling. Tab commits and creates a child. Shift-Return adds a line break. Escape commits and returns focus to the canvas. IME composition is left to Qt while composing text. The editor’s B / I / U toolbar and Ctrl/Command-B / I / U format selected text (or the current word). Normal text selection, copying, pasting, and editor undo remain available inside the title and notes fields.

The Node inspector includes eight shapes, fixed-width wrapping, fill/border/branch/text colors, stroke styles and thickness, font family/face/size, text formatting and alignment. Style edits apply to all selected nodes as one undoable command; mixed values are labeled. Reset to theme clears custom styling and title formatting while keeping title content. The Node inspector also includes a task flag, completion state, folding and notes. Press **Apply notes** to save notes to the selected node before selecting another node. The outline selects nodes; double-click an outline row to edit.

## Document and layout

- Ctrl/Command-O opens a JSON document.
- Ctrl/Command-S opens the save dialog.
- Ctrl/Command-Z and the platform redo shortcut undo/redo document changes outside text fields.
- The Map inspector switches Horizontal / Vertical / Compact layout, spacing and branch line style.
- Spacing is disabled in manual and Compact modes. Compact uses automatic placement.
- Turning Manual placement off restores automatic layout.
- Open, Save and Export PNG also have toolbar buttons.

## Measurements

The stats footer and sample toolbar have been removed. Performance measurements remain available through `--benchmark` and `--render-benchmark`; these measure parts of the pipeline, not end-to-end input latency.

Controls depend on the canvas implementation; platform-specific pointer gestures and drag semantics should be checked against the main README's verified test results. This is a layout and interaction lab, not a production MindNode file-format implementation.

## Live test replay

The `ui_test` executable loads the actual application QML and drives its own window. Set `MINDMAP_LIVE_TEST_MS=1600` to maximize that test window, label each stage in its title bar and pause between visible steps. Leave the variable unset for the normal fast suite. The replay covers editing, rich-text formatting, layout switching, folding, manual subtree dragging, automatic reparenting, undo and rejected-draft preservation. It does not operate an already-open document window.

## Themes

Open Inspector → Themes. Click a preview or focus it and press Space to apply. The first four are Beach Day, Holographic, Retro and Arcade; Lab is the legacy option. Application is one undoable document action and persists through Save/Open. It keeps layout, zoom and hierarchy intact. Invalid/composing title drafts block application; pending notes remain available for Apply notes. Explicit rich-text colors are preserved.

## Toolbar icons

Document, branch, formatting and canvas toolbar actions use locally bundled Lucide icons from Iconify. Hover or keyboard-focus a button for its text tooltip. Labels remain available to assistive technology. PNG export confirmation appears as a temporary message above the canvas controls.
