# Initial Prompt
Search automatically after a typing delay and highlight matching letters with theme-appropriate colors. The first result must be selected automatically, so the next Enter advances to the second result.

# Plan
Debounce the query by 300 ms, automatically visit result zero, and preserve Enter cycling. Share fuzzy matching between result ranking and character positions. Paint transient text selections with contrasting colors and clear them when the query changes or search closes. Verify timing, cycling, rich-text preservation, and highlight cleanup.

# Proposed Next Steps
Restart the rebuilt macOS app to load automatic search and matching-letter highlights.

# Implementation Summary
Search now runs 300 ms after the last text change and automatically centers/flashes the first result. Enter afterward advances to the second result, continuing and wrapping normally; pressing Enter before the timer expires immediately executes the pending query. Matching characters in the active node title are highlighted using contrasting purple/white or gold/dark colors based on its surface. Shared fuzzy matching preserves original character offsets through case/accent normalization and supports substring, abbreviation, and typo matches. Highlight rendering uses paint selections, preserving saved rich text and geometry. Highlights clear on query changes and close. Updated controls and the native shortcut reference.

Rebuilt macOS. Focused engine, canvas, and UI tests passed. Native macOS UI tests verify the debounce, automatic first result, next-result Enter, centering, zoom preservation, and close behavior. Canvas tests verify accent/abbreviation offsets, changed rendering, unchanged document/revision, and exact image restoration after clearing. No packages were built.
