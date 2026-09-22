# Clear misleading saved-login connection feedback

## Initial Prompt
Investigate and fix a saved-password startup appearing stuck on Connecting, with the Share dialog already showing Signed in and Online.

## Plan
Trace automatic-auth status and dialog feedback signals, reproduce misclassification with a regression test, separate progress from action errors, build and run the quiet checks, then restart normally.

## Proposed Next Steps
Use the reopened development build. The installed /Applications copy has not been replaced; a future installer build will include this fix.

## Implementation Summary
- The screenshot showed successful login with stale feedback. ShareClient::sessionMessage was forwarded both to shareStatus and operationFailed. The dialog rendered Connecting as a red error. Automatic login success did not clear it because no manual signin action was pending.
- Session messages now update only live connection status; successful sign-in or renewal replaces that status with Online. Genuine ShareClient errors still emit operationFailed for action feedback. No credentials or production connections were used in tests.
- Added a regression for connecting → automatic signed-in → offline retry → renewed online, verifying zero operation errors for progress and preserving the error signal for a failed invitation. It failed before the fix and passed afterward.
- dev.py build succeeded; dev.py check passed all 11 suites; git diff --check passed.
- The installed app accepted the normal quit request. Opened build-macos/mindarchy.app with the fix; no force quit, installation replacement, packaging, signing, commit or publication.
- Preserved previously staged homepage sorting and weekly-template test work.
