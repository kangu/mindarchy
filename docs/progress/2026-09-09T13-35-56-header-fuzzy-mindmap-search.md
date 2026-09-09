# Initial Prompt
Add fuzzy search within a mind map. Repeated Enter visits the next result by centering and briefly flashing it. Search must be accessible from the header.

# Plan
Add a fuzzy search API covering all nodes and a canvas result-focus operation. Expose an inline header search field, result counter, Enter cycling, and Escape dismissal. Add a brief non-interactive highlight and update the native shortcut reference. Validate matching, hidden results, cycling, centering, zoom preservation, and the complete app test suite.

# Proposed Next Steps
Restart the rebuilt macOS app and use the header magnifying glass or Command-F. No installer or remote deployment requested.

# Implementation Summary
Added header search with case/accent-insensitive matching over node titles, notes, and date entries. Matches include abbreviations and minor spelling errors, rank exact matches first, and preserve tree order for ties. Enter cycles and wraps; no matches leave the view unchanged. Each match is selected, centered at the existing zoom, and flashed with a fading contrast-aware outline. Folded ancestors expand through an undoable state change. Queries are not persisted. Command-F/Ctrl-F opens search; Escape closes it. Updated the native Help shortcut table and controls documentation; Fit Map retains 0 and the zoom menu.

Rebuilt macOS. Engine tests cover fuzzy text, notes, accent normalization, no matches, and folded/date nodes. UI tests cover header access, repeated Enter/wrap, focus, centering, unchanged zoom, flash lifetime, no matches, Escape, and the Find shortcut. Focused UI checks passed on native macOS. All six CTest suites passed in 50.89 seconds. git diff --check passed.
