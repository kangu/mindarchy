# Windows 11 installer and VM verification

## Initial Prompt
Add first-class Windows 11 deployment support to Mindarchy, leaning toward the macOS document conventions. Produce a self-contained EXE/MSI that can run without a developer installation, install and launch it in the testing VM, and provide actual main-window screenshots. Use the existing Inno Setup installation. The user subsequently requested manual local builds without GitHub CI and a Windows menu bar revealed with Alt. The open-source/Linux-first branding request was explicitly withdrawn.

## Plan Followed
1. Inspect the available Parallels Windows 11 VM and installed Inno Setup. Use a temporary, user-started shared-folder worker because Parallels Standard does not permit guest command execution.
2. Add Windows native save/close dialogs, shared document recovery, resource metadata, Ctrl shortcuts, and Alt-controlled File/Help menus with a shortcut table.
3. Add local PowerShell Qt bootstrap, MSVC/CMake deployment, Inno Setup packaging, and installed-runtime verification.
4. Compile and run Windows and macOS tests; correct Windows font fallback, atomic-save test file locking, font discovery for offscreen rendering, deployment of the offscreen plugin, PowerShell checksum/exit-code handling, and timing-sensitive VM test assumptions.
5. Install, uninstall, reinstall, inspect runtime modules, open an `.omm` through Explorer, export PNGs, simulate a crash after confirmed recovery checkpointing, reopen and save, verify overwrite and explicit close, and capture actual Windows screenshots.
6. Verify installed/tested binary hashes and document artifacts and limitations.

## Proposed Next Steps
- Use `scripts/release-windows.ps1 -QtDir C:\Qt\6.11.2\msvc2022_64` for future manual releases; see `docs/platforms/windows.md` for prerequisites and parameters.
- Optional public-release work: publisher signing, additional pristine native x64 hardware coverage, and a separate native ARM64 build.

## Implementation Summary
- Installer: `artifacts/windows/Mindarchy-0.1.0-windows-x64-setup.exe` (about 46 MiB), unsigned, per-user, bundled Qt/QML/platform plugins and MSVC runtime, Start menu entry, optional desktop shortcut, `.omm` association, and uninstall support. No GitHub CI workflow is included.
- Installer SHA256: `5d3ee870f4e92ab0892957c5b27a40f0fad59ec2dff641f1d8a393149a844dc5`.
- Installed and tested executable SHA256: `ad0bdd07c22a6ef071e59e0a21da1f9a5fb205f57a5ce2d0c05cdf2ffd61fb4c` (identical).
- Toolchain: MSVC 2022 x64, Qt 6.11.2, existing Inno Setup 6.4.3. Qt bootstrap was executed successfully within Windows.
- All six Windows suites passed in 89.86 seconds: engine, canvas, UI, placement, preview, and native Windows dialogs. The placement suite skips three macOS-only cases. The six macOS suites passed in 51.81 seconds, with the revised canvas timing assertion additionally rerun successfully.
- Installed acceptance passed: clean install/uninstall/reinstall; changed-association and document preservation; native GUI startup with developer environment removed; app-local runtime modules; PNG export including offscreen plugin; Explorer file opening; native save; overwrite without another picker; crash recovery of unsaved text; Ctrl+W removes the recovery entry.
- Windows menu bar starts hidden, Alt reveals it, and Alt/Esc/action/canvas click dismiss it. Help exposes the current Windows shortcut table. The standard Windows caption remains for snap/minimize/maximize/close. Windows node measurement/rendering/editing use Segoe UI.
- Proof: `artifacts/windows/verification/` contains `windows-11-desktop.png`, `windows-recovered-document.png`, `windows-alt-menu.png`, `installed-main-window.png`, PNG exports, test logs, module paths, and JSON acceptance reports.
- Tested environment: existing Windows 11 Home build 26200 ARM64 Parallels VM using x64 emulation, not a pristine native x64 PC. Runtime isolation and a clean application directory were verified; no claim of pristine-OS testing is made.
- The installed Windows application remains open for manual testing. The latest macOS build was also restarted gracefully after its regression checks. The temporary helper was moved to ignored VM artifacts, its desktop launcher was removed, and its stop marker was set.
