# Document recovery implementation plan

Goal: macOS Cmd+Q restores every document window and unsaved edits without save prompts; periodic recovery protects against crashes.

Architecture: atomic, private recovery envelopes in the existing Application Support session directory, separate from user .omm documents. Envelopes retain original path, saved baseline, document content, and unfinished UI drafts. Existing cross-process quit votes wait for successful snapshots. Per-window recovery identities retain placement; viewport and draft state restore after canvas initialization.

1. Add engine recovery round-trip and failure tests, then implement validated envelope loading and atomic writing without changing document identity or dirty state.
2. Extend session discovery to recover snapshots while preventing duplication of live windows; remove recovery only after explicit window close/discard.
3. Integrate macOS startup, periodic snapshots, and quit checkpointing. Capture text, notes, date drafts and viewport; preserve per-window placement.
4. Verify focused tests, full relevant suites, development build, and isolated startup/recovery behavior. Document guarantees and bounded crash-loss interval. Do not build packages.
