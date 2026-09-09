# Initial Prompt
Move the floating zoom bar into the header left of the far-right toggles.

# Plan
1. Relocate existing controls, preserving actions and tooltips.
2. Remove the floating container and adjust responsive spacing.
3. Build and verify.

# Proposed Next Steps
No further changes required.

# Implementation Summary
Moved all zoom controls into the header before the Outline and Inspector toggles. Removed the floating canvas bar. Adjusted responsive title visibility. macOS rebuild succeeded; toolbar placement, percentage reset, and responsive spacing tests passed. Visually verified header placement and clear canvas. git diff --check passed.
