# Start sharing attachment fix

## Initial Prompt
I click on Start Sharign, but nothing happens on the UI, I cannot select the users to which i want to share to. I see some documents being created in the couchdb

## Plan
1. Trace server create, coordinator attachment and dialog invitation visibility.
2. Reproduce attachment failures in regression tests.
3. Separate sharing activation from sidecar persistence and verify.

## Next Steps
Commit/push this client fix and rebuild/reinstall the Omarchy app; restart the rebuilt macOS app after saving work. Existing created maps remain accessible through Shared with me. The current invitation UI accepts collaborator account IDs; it does not offer an account directory picker.

## Implementation Summary
Fixed a reproduced Start sharing failure: a successful server create was discarded by the UI when writing the local .share sidecar failed. Map identity and owner permissions now activate independently of local persistence; success follows attachment and failures display an actionable warning. Unsaved maps no longer read/write a working-directory .share file, and saving later persists the association atomically. Repeating Start sharing for an attached map does not create another server map. Regression tests failed before the fix and passed afterward, covering blocked sidecars, unsaved maps, save-after-share and repeated sharing. All 12 fast suites passed; macOS build and diff checks passed. Normal app restart was cancelled (-128); running session preserved. No server or database data was modified.
