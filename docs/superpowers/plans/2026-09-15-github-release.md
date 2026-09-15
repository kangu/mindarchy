# GitHub Release Implementation Plan

**Goal:** Configure GitHub releases and upload the packaged Mindarchy binaries with verified checksums.
**Architecture:** A Python standard-library CLI discovers local platform artifacts and uses the authenticated GitHub CLI to create a draft, upload assets, and verify them before optional publication.
**Tech Stack:** Python 3.9+, unittest, GitHub CLI.
**Spec:** User request: make a script that configures a GitHub release and uploads the proper binaries for users to download.

## Constraints

- Default repository is kangu/mindarchy; version is explicit.
- Require every selected platform; support an explicit platform subset for previews.
- macOS uses completed release manifests, Windows uses EXE installers, Omarchy uses Arch packages.
- No real release is created during implementation/testing.
- Preserve existing README edits and screenshots.

## Steps

- [x] Add tests in tests/test_release_github.py for exact-version selection, latest completed macOS build selection, checksum rejection, missing platforms, and draft/upload/publish failure behavior. Run with `python3 -m unittest discover -s tests -p test_release_github.py -v` and observe the missing implementation failure.
- [x] Implement scripts/release-github.py: collect artifacts, generate SHA256SUMS and release notes, dry-run without gh, draft creation and explicit resume, download verification, explicit publication. Repeat the focused tests.
- [x] Document authentication, existing tag requirement, local collection of remote builds, platform subsets, dry-run, draft, resume and publish in docs/github-release.md and link from README.md.
- [x] Run a dry-run against the actual 0.1.4 macOS artifacts, check default all-platform failure, run tests and git diff --check. Persist the implementation summary in docs/progress.
