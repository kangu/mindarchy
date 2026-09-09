# Initial Prompts
Build the macOS installer. Close untouched new documents without a confirmation.

# Plan
1. Run the existing release workflow.
2. Diagnose smoke-test shutdown and use a saved fixture.
3. Add close-behavior regression tests and fix dirty-state and untouched editor handling.
4. Rebuild the release with the fix and verify actual installer payload and DMG.

# Proposed Next Steps
Install the verified PKG or use the DMG.

# Implementation Summary
Untouched new documents now close without confirmation: hasUnsavedChanges uses actual content changes, and committing an untouched rich-text editor no longer changes plain text into HTML. Modified documents still prompt; undoing back to the initial content removes the prompt. Engine and window-level regressions passed. Built unsigned arm64 macOS 0.1.0 PKG and DMG including the fix. All six release CTest suites passed. App dependencies/signatures, extracted installer payload, native QML launch/screenshot, document preview renderer, and DMG integrity were verified. Updated release smoke testing to open a saved fixture and allow 3000 ms before shutdown. Artifacts: /Users/user/Projects/mindmap-blue/qt-prototype/dist/macos/0.1.0/arm64/20260909T071942128857Z. No system installation was performed.
