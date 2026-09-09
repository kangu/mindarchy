# Initial Prompt
Add a native macOS Help menu with a Keyboard Shortcuts entry opening a separate table of existing shortcuts. Update agents.md to require maintaining this reference whenever shortcuts change.

# Plan
Audit implemented canvas, editor, document, and window shortcuts. Build a native AppKit menu and reusable resizable window with a scrollable three-column reference. Verify opening, reuse, close, and reopen; document shortcut maintenance.

# Proposed Next Steps
Restart the rebuilt macOS app to access Help → Keyboard Shortcuts. No installer or Omarchy deployment is needed for this macOS-only feature.

# Implementation Summary
Added native Help → Keyboard Shortcuts… and a separate window with 35 rows covering document, canvas, node editing, text fields, date entries, and window controls. Uses native adaptive colors, alternating table rows, and macOS key symbols. Repeated activation reuses the window; Command-W closes only the reference window. Updated agents.md with a Keyboard shortcuts section requiring the reference, descriptions, contexts, and related documentation/verification to stay synchronized with every shortcut addition, modification, or removal.

Rebuilt macOS successfully. Native integration tests passed for the new menu, table, window reuse, Command-W closing, and reopening, alongside existing native window/menu tests. Inspected a rendered screenshot of the table.
