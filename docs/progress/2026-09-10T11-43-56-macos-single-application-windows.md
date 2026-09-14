# Initial Prompt
Group all Mindarchy windows in macOS App Exposé under a single Dock icon; provide the open mind map list through the Dock context menu with window activation.

# Plan
Trace the per-document process lifecycle. Introduce a macOS application document manager and launch forwarding. Route native menus to active windows, retain per-document recovery/placement, and use AppKit’s standard Dock window registration. Add lifecycle regression tests, rebuild, and restart the live application safely.

# Next Steps
Use the single Dock icon to switch documents and App Exposé to view them together. Existing Windows and Omarchy deployments remain unchanged.

# Implementation Summary
Implemented a macOS application-level document manager with one shared QML engine and independent per-document controllers, recovery, placement, and viewport state. Interactive New/Open/Finder/recent-file requests now create or activate windows in that process; a user-local locked IPC endpoint forwards subsequent launches. Global File commands follow the active window, and Window cycling uses stable window identities. AppKit’s native window registration supplies the standard Dock document list and application grouping; a reopen handler creates a blank window after the final document closes. No duplicate custom Dock list is appended.

Preserved Cmd+W removal from startup restoration and coordinated Cmd+Q recovery. Added a recovery validation fix: a directory at a snapshot path can no longer be mistaken for a valid cached recovery file. A failed checkpoint cancels the entire quit and keeps all documents open.

Verification: macOS build succeeded. Native application integration tests cover isolated editing, duplicate-file activation, a forwarded second process, native window registration/selection including minimized windows, cycling, closing a single window, failed-checkpoint quit cancellation, successful draft recovery, startup restoration, closing the final window, and Dock reopen. macwindow, engine, canvas, UI, and window-placement suites passed; focused recovery UI tests passed after the checkpoint change. Final native application integration test passed after accounting for process-start and minimize-animation timing in its harness.

Restarted the live app gracefully after backing up its recovery session. Verified four restored windows registered under one process and all listed in the native Window menu; selected another document successfully. The Dock window-list backing and activation were verified through AppKit tests; the Dock’s full context menu and App Exposé overlay could not be directly captured with the available app-scoped computer-use surface. No Windows/Omarchy deployment, installer, or git commit was performed.
