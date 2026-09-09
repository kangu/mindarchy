# Initial Prompt
On macOS, Cmd+Q should preserve unsaved documents in local recovery storage and reopen the same windows on startup; protect against crashes and accidental quits.

# Plan
Implement validated atomic recovery envelopes, integrate multi-process session discovery and quit votes, capture draft UI and viewport state, preserve per-window placement, test crash/write-failure/native-quit behavior, and update documentation.

# Proposed Next Steps
Use the rebuilt macOS development app. Installer packaging remains paused.

# Implementation Summary
Implemented macOS recovery snapshots and prompt-free coordinated Cmd+Q. Atomic owner-only recovery envelopes retain document content, original file identity, saved baseline, unfinished text/notes/date drafts and viewport without overwriting .omm files. A one-second timer skips unchanged snapshots; quitting waits for successful checkpoints from all windows and aborts on storage failure. Startup discovers orphaned snapshots, reuses per-window identities and restores drafts, viewport and placement with existing unavailable-display fallback. Explicit Cmd+W/save/discard flow remains, removing the closed window recovery. New windows retain the existing default placement behavior. Omarchy quit behavior is unchanged. Rebuilt macOS development app. All six suites passed after rebuilding concurrent layout work; focused native Cocoa Cmd+Q and failure tests passed. Abrupt-process-exit recovery, multiple-window discovery, original-file preservation, untitled/clean/edited state, and draft restoration verified. No packages built. Crash recovery uses the latest completed snapshot; very recent changes or blocked UI/storage can exceed the normal one-second interval.
