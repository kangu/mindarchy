# GitHub release publisher

## Initial Prompt

I want to make a script that configures a release on github and uploads all the proper binaries to that so it can be downloaded by users.

## Plan

1. Inspect existing platform artifact formats and write regression tests.
2. Implement a Python publisher using GitHub CLI with draft creation, installer validation, checksums, resume, and verified publication.
3. Document setup and remote artifact collection; link the guide from README.
4. Test mocked release workflows and dry-run against existing installers.

## Proposed Next Steps

Install GitHub CLI and authenticate, collect the selected platform builds, and push a tag at the source commit used to build them. Run the script to create a draft; review it, then resume with --publish. Current macOS 0.1.4 installers remain unsigned and not notarized.

## Implementation Summary

Implemented scripts/release-github.py with exact-version installer selection for macOS, Windows and Omarchy; macOS manifest checksum validation; generated download notes, SHA256SUMS and public asset metadata; offline dry-run; draft-first uploads; explicit resume without overwriting differing assets; round-trip download verification; and explicit publication. Added the release guide and README link. All 16 focused tests pass. The real 0.1.4 macOS PKG and DMG passed dry-run validation; the default all-platform check correctly rejected missing Windows binaries. git diff --check passed. GitHub CLI is not installed, so no live GitHub operation was attempted or release created. App restart is unnecessary for this tooling-only change.
