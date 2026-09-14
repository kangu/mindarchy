# Windows Release Implementation Plan

**Goal:** Produce a self-contained Windows 11 installer and verify its installed main window in the available VM.

**Architecture:** Keep the Qt engine and QML interface shared. Add Windows native dialogs and recovery wiring, a PowerShell CMake/deployment pipeline, and a per-user Inno Setup installer. Use the existing Windows VM and its Inno Setup compiler; do not depend on Parallels Pro commands.

**Tech stack:** C++20, Qt 6.11, CMake, MSVC x64, PowerShell, Inno Setup.

**Spec:** User request in this task: Windows first-class deployment, macOS-like document behavior, fresh Windows 11 runtime compatibility, actual VM screenshots.

## Constraints
- Preserve `.omm` as portable JSON and existing macOS/Linux behavior.
- Bundle Qt QML imports, plugins, and compiler runtime; no developer PATH dependency.
- Per-user installation, Start menu shortcut, optional desktop shortcut, registered `.omm` open command, uninstall support.
- Windows uses standard Ctrl shortcuts and native window chrome/dialogs.
- Recovery stores local snapshots outside original documents. Explicit close retains save/discard/cancel.
- Do not claim a pristine OS or native x64 validation when the available machine is an existing ARM64 VM.

## Steps
- [x] Inspect VM toolchain and existing Inno Setup installation through available UI/shared files.
- [x] Implement Windows native close/save adapters and enable the shared recovery mechanism.
- [x] Add version metadata, executable installation target, and repeatable release PowerShell script.
- [x] Add Inno Setup definition with per-user registration and packaged runtime.
- [x] Compile and run applicable engine, canvas, UI, preview and placement tests on Windows; fix platform failures.
- [x] Build installer with existing Inno Setup; install into a clean application directory.
- [x] Launch installed binary with Qt/build tools removed from PATH, test document open/save and recovery, capture main-window screenshots.
- [x] Document platform differences, build instructions, verification limits and artifact hashes. Persist implementation summary in docs/progress.
