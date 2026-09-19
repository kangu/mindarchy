# Welcome screen

Normal launches display a full-window homepage with the last six saved maps opened or saved by Mindarchy. Three columns and two rows are used on desktop-sized windows; the grid adapts to two columns below 960 px and one below 620 px. Empty slots create a new map.

Each card renders a snapshot of the saved map using the same renderer as exports. Rendering runs asynchronously through Qt's image provider. The cache key includes file modification time and size. The map's background fills the preview, preserving the complete map rather than cropping branches. No thumbnail files or map contents are uploaded.

New Map begins editing in the existing window. Recent cards and Open Map restore each file’s last zoom and map center, including after restarting the app. Camera state is stored locally, outside the map file and undo history; maps without a saved view fit the window. Recent cards and Open Map replace the untouched welcome editor, without adding a blank tab. Existing New/Open shortcuts are available. Missing files remain marked; invalid files show an error without leaving the homepage. File-dialog cancellation leaves the homepage intact.

Opening an explicit document or launching with `--new` bypasses Home. Unfinished recovery snapshots still restore, including pending title/notes/date edits and tab ordering. Clean saved sessions no longer reopen automatically. The untouched homepage is excluded from recovery checkpoints. Dock reopening with no windows presents Home.

The app uses the existing shell palette, native caption control spacing, keyboard focus borders, subtle hover feedback and a short preview fade. Native platform runtime checks remain part of release validation; development verification uses fast tests and selected offscreen startup/UI cases.

## Keyboard interaction

Arrow keys navigate the responsive grid. Tab/Shift+Tab reach all enabled actions and cards, Enter/Space activate, Home/End jump to the first/last card, and Escape returns to New Map. Initial focus is the first available recent card (New Map for an empty history). Missing files are skipped. Focus borders and automatic scrolling keep the selected control visible. File-picker cancellation restores focus. Platform New/Open shortcuts remain available, and Help → Keyboard Shortcuts lists the welcome controls.
