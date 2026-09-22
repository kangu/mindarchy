# Welcome maps sorted by last modification

## Initial Prompt
“Most recent first” is not enforced; sort welcome maps based on last modified date.

## Plan
Inspect the homepage data source, write a regression with conflicting open/modified orders, sort before limiting to six, update keyboard expectations for unavailable files, and verify with focused and fast checks.

## Proposed Next Steps
Open the updated development build after closing the running installed app. The normal quit request to /Applications/Mindarchy.app returned User cancelled, so the session was preserved. The installed app was not replaced.

## Implementation Summary
- Engine::recentMaps previously took the first six entries from last-opened history. It now sorts all history candidates by filesystem modification timestamp descending before selecting six.
- Available maps precede unavailable entries. Equal timestamps and unavailable entries preserve history ordering through a stable sort. Metadata is reread on each welcome refresh; existing focus/activation refresh paths pick up external file modifications.
- Added a regression proving sort-before-limit, opening an older file does not reorder by access, external modification changes ordering, missing-file placement, and stable timestamp ties. Observed the expected failure before fixing production code.
- Adjusted welcome keyboard tests for missing cards at the end and updated homepage documentation.
- App build passed. Focused engine tests: 4 passed including setup/cleanup; focused welcome UI tests: 4 passed including setup/cleanup. dev.py check passed all 11 suites. git diff --check passed. No native UI replay, installer, signing, or publication.
- Preserved the earlier weekly-template synchronization change and progress note already present in the workspace.
