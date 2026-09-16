# Automatic GitHub release tag

## Initial Prompt
Automate tag creation so a GitHub release does not require a manual `git tag` / `git push`.

## Plan
Have `scripts/release-github.py` resolve HEAD (or `--tag-commit`), create an annotated GitHub tag when it is missing, reuse it when it already points at that commit, and refuse to move a tag that points elsewhere. Update the release guide and tests.

## Next Steps
Install GitHub CLI, authenticate, and run a dry-run then a real draft when installers are staged.

## Implementation Summary
The publisher tags `v<version>` through the GitHub API using `gh` (same credentials as the release). Default commit is repository HEAD; `--tag-commit SHA` pins the build. Existing matching tags are reused; mismatched tags abort. `gh release create` still uses `--verify-tag`. Dry-run prints the commit it would tag and does not call GitHub.

19 publisher tests passed. Docs updated. App rebuild not required.
