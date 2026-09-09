# Initial Prompt
Make automatic connections more natural using parent-relative placement, with no overlapping nodes.

# Plan
Reproduce the global-column gap with a mixed-size layout test, use parent-relative forward positions, retain subtree separation, and run engine/canvas regression suites in an isolated build directory.

# Proposed Next Steps
The main development build will pick up this source change on its next rebuild.

# Implementation Summary
Automatic horizontal and vertical layouts now place children a fixed spacing gap beyond their own parent, rather than beyond the widest node at that depth. Existing disjoint subtree bands retain non-overlap. Manual base geometry and Compact placement remain unchanged. A mixed-size calendar/text regression reproduced the old 257-vs-36-pixel gap, then passed after the change across all three spacings and both directions with pairwise overlap checks. Full engine and canvas suites passed in a separate build-layout-check directory to avoid disrupting the main task build.
