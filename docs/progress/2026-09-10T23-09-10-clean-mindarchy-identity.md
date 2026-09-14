# Initial Prompt
Remove the remaining Blue Mindmap identity and use Mindarchy artwork. Use a single Mindarchy/session storage folder. Since this is an unreleased prototype, remove legacy migration and compatibility code.

# Plan
Rename active bundle, launcher, document-type and packaging identifiers; verify icon resources; simplify storage to Mindarchy/session without legacy upgrades; rebuild and test on macOS.

# Next Steps
Quit and reopen the running app manually to load the final simplification. The computer-use connector inventories org.mindarchy.app correctly but still resolves control requests to blue.mindmap.lab, preventing an automated safe restart. Historical activity labels may remain cached.

# Implementation Summary
Active Qt application and packaging use org.mindarchy.app, org.mindarchy.omm, the Mindarchy document format, and current green icon assets. Renamed Linux launcher, thumbnailer and icon files; updated packaging metadata, scripts, examples and docs. No old identity strings remain in active source, packaging, scripts or tests.
Storage is ~/Library/Application Support/Mindarchy/session on macOS. Removed legacy settings/session migration, old document-format acceptance and their tests per the final request. Existing disk data was not deleted. Prior in-session migration had already run before its removal was requested.
macOS build and bundle identity/icon verification passed. Engine and preview suites passed. Native application tests initially encountered a focus timeout while computer-use inspection was active; isolated rerun passed. No Linux/Windows packages were built or deployed.
