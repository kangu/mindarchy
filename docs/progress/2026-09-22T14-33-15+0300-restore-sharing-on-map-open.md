# Restore sharing on map open

## Initial Prompt
After sharing a map, i expect that when i close mindarchy and reopen the map again, to be again shared and in a state where it's synced with other potential accounts using it

## Plan
1. Trace attachment persistence and document/authentication lifecycle.
2. Reproduce welcome-screen reopen with a saved sharing attachment.
3. Restore and reconnect per document; preserve previous attachments while switching.
4. Test login ordering, server restoration and map switching; rebuild.

## Next Steps
Save local work and reopen the rebuilt macOS app. Commit/push client changes before rebuilding Omarchy. The saved .omm.share attachment identifies the shared server map; retain it alongside the saved map. No backend rebuild is required.

## Implementation Summary
Fixed sharing restoration for maps opened after coordinator construction, including welcome-screen reopen and recovery opens. Document opening now leaves the previous room and clears document-scoped runtime state without deleting its saved sharing attachment. Document opened restores map ID/role/server, resumes the scoped outbox and joins/fetches state when authenticated; delayed saved-login completion joins through the existing authentication handler. Restoring a different saved server preserves credentials instead of signing out destructively. Added a red/green regression covering fresh coordinator reopen with login before/after open, saved server restoration, switching to an unshared document and returning to the shared document. All 12 fast suites passed; macOS build and diff check passed. Normal app restart was cancelled (-128), preserving the running session. No backend changes or deployment.
