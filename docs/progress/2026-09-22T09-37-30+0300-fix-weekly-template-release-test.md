# Fix intermittent weekly template release test

## Initial Prompt
Investigate and fix VERSION=0.1.6 ./scripts/build_macos.sh failing at ctest (exit 8), with the supplied release log.

## Plan
Inspect the failing release log; reproduce the specific UI failure; capture the click target's geometry; synchronize the test with actual layout readiness; repeat the targeted test and rerun the exact release command through packaging.

## Proposed Next Steps
Use the verified 0.1.6 installer or DMG in dist/macos/0.1.6/arm64/20260922T063009530719Z. These use the command's default unsigned mode. No application restart is needed for this test-only change.

## Implementation Summary
- Compilation succeeded; ctest exit 8 came from weeklyTemplatePickerAddsBranch expecting September 21 but retaining September 7. Other suites passed.
- Reproduced the exact failure on isolated run 5. Geometry diagnostics showed the target row at scene y=278 while its clipped parent was only 199.399px tall (content height 344px). The mouse event landed outside the expanding popup, closing it.
- Removed the fixed 250ms delay and frameSwapped wait. A frame can be presented before nested height animations finish; conversely, a fully settled offscreen scene may not present another frame just because requestUpdate is called.
- The test now waits (bounded to 3 seconds) for the final popup width and positive, visible ancestor geometry with all clipped child content exposed. Existing real Wednesday/Sunday mouse clicks and template creation assertions remain unchanged.
- Validation: 20 consecutive isolated runs passed; the exact VERSION=0.1.6 ./scripts/build_macos.sh invocation then exited 0. All 12 offscreen and 4 native suites passed, as did payload bundle/linkage/signature verification, launch screenshots, document preview rendering, PKG generation/extraction, and DMG creation/verification. git diff --check passed.
- Produced Mindarchy-0.1.6-macos-arm64-unsigned.pkg and .dmg, with release.json, SHA256SUMS, and release.log under the verified output directory. No release checks were skipped or disabled. No product code changes, commit, or publication performed.
