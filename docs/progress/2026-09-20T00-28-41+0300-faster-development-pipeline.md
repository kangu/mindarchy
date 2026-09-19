# Faster development pipeline

## Initial Prompt
Evaluate the build pipeline, make development faster, and avoid slow UI tests disrupting the desktop except when necessary for releases.

## Plan
Inspect build and packaging scripts and measured test timings. Separate app builds, quiet checks, opt-in UI/native tests and release validation. Reuse CMake outputs and validate the new workflow without launching native tests.

## Proposed Next Steps
Profile UI cases before replacing fixed waits. Evaluate Ninja/compiler caching for fresh builds and add hosted per-platform release gates, especially Linux.

## Implementation Summary
Excluded test binaries from normal app builds, added BUILD_TESTING support and explicit fast/full test targets, and introduced a shared incremental development runner. Fast tests run offscreen in parallel. Updated legacy shell runners, README and agent policy; retained full macOS/Windows release checks by default. Avoided repeated CMake configuration when a configured tree is available. Measured warm app build at 3.1 seconds and fast build/check at 5.3 seconds (four passing suites; 2.63 seconds test time), compared with the previous 132.86-second full suite. No native tests or app restarts were performed during this task. Python and shell syntax checks passed; test tiers were inspected with CTest dry runs. Windows release script changes require runtime validation on Windows.
