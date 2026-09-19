# Fix settled camera after welcome reopen

## Initial Prompt
Opening maps from the welcome screen still moves to an incorrect fixed position. Implement a fix and run a test confirming proper camera restoration.

## Plan
Reproduce the actual standalone launch and the pan/close/welcome-reopen lifecycle before changing behavior. Trace delayed camera changes. Fix the proven causes and rerun those regression tests plus fast suites, offscreen.

## Proposed Next Steps
Use the newly rebuilt homepage for further map work. Existing camera values overwritten by earlier versions cannot reconstruct a previously unsaved viewpoint; new interactions now persist normally.

## Implementation Summary
Reproduced two failures before the fix: the standalone --no-window-state launch used the fitted zoom rather than the saved zoom, and welcome reopening initially restored the camera but a deferred new-map callback subsequently entered root editing and moved the view. Decoupled camera persistence from session/window-placement enablement and guarded deferred root editing against an already-opened file. Added screenshot viewport metadata for executable-level diagnostic verification. Regression coverage now launches the actual executable, and separately pans/zooms a map, immediately closes it, reopens it through Home, waits 700 ms for initialization/timers, and verifies zoom/center plus absence of unintended editing. Both previously failing cases now pass, along with two related startup checks and all four fast suites. Release build succeeded. All tests ran offscreen; no full native/UI suite was run. Opened the corrected homepage separately, preserving existing sessions.
