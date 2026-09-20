# Restore map camera from Home

## Initial Prompt
Remember camera position and zoom for every mindmap so reopening resumes where the user left off. Explicitly connect this behavior to homescreen map opening.

## Plan
Trace existing viewport persistence and homepage open paths. Restore on successful document opening, flush outgoing state before replacing a map, remove the extra fit after homepage opening, and verify both recent-card and application open paths offscreen.

## Proposed Next Steps
Use the updated homepage in normal work; retain native cross-platform checks for release validation.

## Implementation Summary
Connected successful document opens to canvas initialization and per-file viewport restoration. Added a pre-replacement signal to flush the outgoing map's pending camera state and suspend captures during loading. Synced local settings before restoration and removed the fit that overwrote remembered views in the welcome-window reuse path. Both recent cards and the homepage Open Map action now use the shared restoration path. New maps and files without a remembered view still fit normally. Camera state remains outside document contents and undo history. Four fast suites passed, including immediate map-switch persistence and invalid-open preservation; targeted homepage-card and application-open checks passed offscreen. Rebuilt the macOS app without running full UI/native tests.
