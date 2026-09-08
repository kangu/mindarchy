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

## Manual branch direction

In Horizontal layout with Manual placement enabled, drag a branch across the root's vertical centerline to change its growth direction. A branch centered left of the root grows left; moving it back to the right restores rightward growth. Descendant positions and connector attachment edges update during the drag, while text remains readable. The drop keeps the preview geometry without a second layout animation. Relative manual adjustments mirror with their parent, and dragging a child within a left-facing branch still follows the pointer normally. The arrangement survives save/open and supports undo/redo. Vertical and Compact layouts keep their existing behavior.

## Date nodes

To convert an existing node, open **Inspector → Node → Type → Date**. The top icon selector offers Text, Task and Date, using the Map tab’s hover, focus and selected styling. Task groups its Completed option directly below the selector; Date groups its calendar settings there. Text has no extra type options. Choose Week or Month inside the Date group. Switching back to Text restores the retained title, and calendar entries remain stored even across save/open. Conversion preserves the branch and notes; Date nodes turn off task/completion status.

Create a normal node, then set its Type to Date in the inspector. New calendars start in the current period; weeks begin on Monday. The heading arrows navigate periods. Inspector → Node also offers Week/Month, Previous/Next, an ISO date and Today.

Click an empty day to create its text entry, or a filled day to edit. Save commits; Cancel discards the draft; Remove clears an existing entry. Assigned days use a themed fill, and their text appears only on hover outside the editor. Entries belong to actual dates and remain stored when navigating away or changing Week/Month. The inspector's day picker provides keyboard access to the editor.

Dragging a day or heading moves the node without opening the entry editor. Date nodes support normal branches, manual placement, folding, themes, undo/redo and JSON save/open. Entry edits keep calendar geometry unchanged. Each entry is limited to 4,096 characters, with at most 3,660 assigned dates per node. Date nodes have no text-title or task-checkbox editing.


### Numeric date entries

If any visible date entry contains only a number, the calendar adds a Sum column to the right. Each week row shows the sum of numeric entries in that row. Month view also shows a Month total underneath; partial weeks count only days in the displayed month. Week view may span month boundaries and counts all seven visible days.

Signed integers and decimal values are accepted, with a dot or comma as the decimal separator and optional surrounding whitespace. Zero activates sums too. Text, units, currency symbols, exponents and grouped numbers are excluded. Non-numeric comments keep their normal fill and hover behavior. Totals are calculated from entries, so edits, removal, undo/redo and period changes update them automatically. The totals column hides when no numeric entries remain in the visible period. Adding or removing the column changes the node's dimensions; subsequent numeric edits keep those dimensions stable. Totals display up to 12 significant digits; an out-of-range sum displays Overflow.

## Window placement

Normal app launches remember window size and position when the window closes. Placement is a local application preference, separate from mindmap documents. Startup checks the saved display identity and rectangle against currently available display work areas. If the display is missing, the saved rectangle is off-screen, or the saved dimensions are invalid, the app opens at the default size centered on the current primary/focused display, reduced to fit smaller screens.

On macOS, window coordinates are restored directly; maximized/fullscreen launches retain the saved normal rectangle. Minimized geometry does not replace it. On Omarchy/Hyprland, floating windows are restored using the compositor's actual coordinates and display scale/reserved areas. Tiled windows stay under Hyprland's layout control, so their exact pixel rectangle may change with other windows. No global Hyprland rules are installed. Both current Lua dispatchers and legacy dispatch syntax are supported. If the compositor API is unavailable, normal compositor placement is used.

`--no-window-state` starts with defaults without updating the stored preference. Benchmark, screenshot and timed test launches also bypass placement persistence so test windows do not overwrite the user's preference. Multiple normal instances share the preference; the last one closed wins.


Platform references: [Qt window positioning limitations](https://doc.qt.io/qt-6/qwindow.html#setPosition), [Hyprland dispatchers](https://wiki.hypr.land/configuring/core/dispatchers/).
