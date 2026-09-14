# Pre-Public-Release Privacy Audit

## Initial Prompt

Scan the current repository and tell me which things need to be removed before pushing to a public GitHub repository so I don't accidentally reveal things about myself or the development machine.

## Plan

1. Inventory tracked files vs ignored files; confirm what a `git add .` would sweep in.
2. Grep tracked tree and full git history (17 commits) for secrets: private keys, tokens, API keys, passwords.
3. Grep for personal/machine identifiers: `/Users/user`, `/home/user`, emails, IP addresses, Tailscale, hardware fingerprints.
4. Inspect `.idea/`, scripts, packaging, examples, screenshots, and benchmark JSON for leaked content.
5. Report findings by severity with removal recommendations.

## Implementation Summary

Scanned all 373 tracked files and all 17 commits (`git grep` over working tree plus `git log --all -p`). No secrets found anywhere: no private keys, tokens, API keys, or real credentials. `publicKeyToken='6595b64144ccf1df'` in CMakeLists.txt is Microsoft's well-known Windows manifest constant; `user:password@example.com` in tests/engine_test.cpp is a deliberate URL-validation fixture.

Found infrastructure and machine disclosures that must be scrubbed:

- **Tailscale IP + SSH user (High):** `omarchy-host` with user `user` and `/home/user/mindmap-qt-lab` in README.md (lines 7, 17, 253), docs/plan.md, docs/verification.md, and 6+ docs/progress files. Also present in commit history.
- **Local paths (Medium):** `/Users/user/Projects/mindmap-blue/qt-prototype/...` and `/Users/user/Qt/6.11.2/macos` in docs/progress/, docs/theme-engine-brief.md, docs/theme-engine-report.md, docs/verification.md. Hardware fingerprint `Intel Core i7` in docs/verification.md.
- **docs/progress/ (74 files):** internal agent work logs containing all of the above plus PIDs, Tailscale auth events, and deploy timestamps. Recommend excluding the whole folder rather than scrubbing file-by-file; same for docs/plan.md, docs/theme-engine-brief.md, docs/theme-engine-report.md.
- **Commit author identity:** all 17 commits are `Mindarchy contributors <noreply@example.com>` — a personal choice; rewrite with git filter-repo pre-push if anonymity is desired.
- **.idea/ tracked (11 files):** contents verified clean (`$PROJECT_DIR$` placeholders, no paths), but recommend untracking and adding `.idea/` to .gitignore as hygiene.

Verified clean: scripts/, packaging/, CMakeLists.txt (no hardcoded personal paths); examples/*.omm and artifacts/*.png (generic demo content only); artifacts/*.json (benchmark numbers only); LICENSE.md ("Mindarchy contributors", no personal name); untracked dirs (build-*, dist, cmake-build-debug self-ignores, .DS_Store ignored) cannot be swept in by `git add .`.

## Next Steps

1. Decide: keep commit history (rewrite with `git filter-repo` to replace `omarchy-host`, `user@omarchy-host`, `/home/user`, `/Users/user` strings) or start a fresh single-commit repo (simplest).
2. Scrub README.md Omarchy sections to use generic placeholders (`user@host`).
3. Remove docs/progress/, docs/plan.md, docs/theme-engine-brief.md, docs/theme-engine-report.md, docs/verification.md from the public tree (or scrub and keep).
4. `git rm -r --cached .idea` and add `.idea/` to .gitignore.
5. Optionally switch to a GitHub noreply email before the first public push.
