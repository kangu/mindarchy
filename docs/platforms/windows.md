# Windows release tooling

Target: Windows 11 x64. Windows ARM64 may run the x64 installer using Windows emulation; native ARM64 is not a separate build target yet.

## Build

Build locally; no GitHub remote or CI service is required.

Use a Windows build machine with CMake, Ninja, Visual Studio C++ Build Tools (MSVC x64 and Windows SDK), Qt 6.11 MSVC x64 including Qt Quick/Quick Controls/Test, and Inno Setup 6.3 or newer. Use an existing Inno Setup installation where available.

```powershell
# Run once if Qt is not already installed:
powershell -ExecutionPolicy Bypass -File scripts\bootstrap-windows-qt.ps1

powershell -ExecutionPolicy Bypass -File scripts\release-windows.ps1 `
  -QtDir C:\Qt\6.11.2\msvc2022_64 `
  -InnoCompiler 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' `
  -Version 0.1.0
```

The script imports the MSVC developer environment, builds and tests, stages the executable, runs `windeployqt` with the QML source directory, copies the app-local compiler runtime and license texts, verifies required DLLs, and invokes Inno Setup. Output: `artifacts/windows/Mindarchy-0.1.0-windows-x64-setup.exe` plus SHA256. Release artifacts are not committed.

Qt installations must include their redistributed license texts in `LICENSES` (or the Qt installer `Licenses` directory). Build provenance and the exact Qt version must accompany the release.

## Platform choices

| Behavior | macOS | Omarchy | Windows |
| --- | --- | --- | --- |
| Window chrome | Integrated macOS toolbar | Compositor-managed | Integrated header controls with native move/resize |
| Shell theme | App shell | Live Omarchy adaptation | App shell; document themes remain independent |
| File picker | Native macOS | Qt platform picker | Native Windows picker; `.omm` filter |
| Unsaved explicit close | Native sheet | App dialog | Windows Save / Don't Save / Cancel dialog |
| Recovery | Local snapshots | Existing session behavior | Local snapshots using shared recovery controller |
| Quit all | Application menu / Cmd+Q | Existing Quit action | Alt → File → Exit Mindarchy |
| Shortcuts | Command conventions | Control conventions | Control conventions; Alt reveals File/Help, Help → Keyboard Shortcuts |
| Viewport / placement | Local per-file state | Local per-file state | Local per-file state and display fallback |
| Documents | `.omm` JSON | Same `.omm` JSON | Same `.omm` JSON and Explorer association |
| Preview | Quick Look extension | Thumbnailer | CLI PNG preview export; Explorer preview handler not implemented |
| Installation | App/DMG | Linux deployment | Per-user Inno Setup EXE, Start menu, optional desktop shortcut |

The Windows menu bar is hidden by default. Alt reveals it; another Alt, Esc, choosing an action, or clicking the document dismisses it. The separate Windows caption title/icon are hidden. Minimize/maximize/close buttons share the header’s right edge; empty header space starts a system drag, and double-clicking toggles maximize. Native resize borders remain enabled. Windows uses Segoe UI consistently for node measurement, editing, rendering, and PNG exports.

Explicit Close window discards its restore entry after confirmation. Exit Mindarchy checkpoints and retains open documents for restart. Recovery snapshots are separate from the original `.omm` file. Uninstall preserves documents, settings, and recovery data.

The installer registers Mindarchy in Open With and sets the `.omm` association only when empty or already Mindarchy. Windows protects the user's chosen default application; use Open With if another application is already selected.

## Installed validation

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test-windows-installed.ps1
```

This launches the installed executable with developer paths and Qt environment overrides removed. It checks exit status, missing QML/plugin errors, and saves a real application screenshot. This is separate from a full desktop screenshot proving the VM environment.

Manual acceptance: create/open/save `.omm`, overwrite without another picker, cancel/discard/save on explicit close, quit/restart with unsaved recovery, maximize/relaunch, resize without zoom change, check search and repeated zoom actions. Test the installer in a fresh Windows user or pristine VM without Qt/Visual Studio installed before calling a release fresh-system verified.

Signing is not configured by this tooling. A public release needs the publisher's signing configuration; an unsigned build must be labeled as such.

## Current validation status

Validated on Windows 11 Home build 26200, ARM64 Parallels guest running the x64 binary:

- MSVC 2022 / Qt 6.11.2 compilation and all six Windows test suites passed (89.86 seconds).
- Native Save/Don't Save/Cancel choices and native `.omm` save picker exercised through actual Windows dialogs.
- Qt bootstrap script ran successfully inside Windows, including archive checksums, extraction, and redistributed license files; qmake reports 6.11.2.
- Final EXE produced with the VM's existing Inno Setup 6.4.3; clean application-directory install, uninstall, and reinstall passed. Uninstall preserved the document and a changed `.omm` association.
- Installed runtime launches with developer PATH and Qt overrides removed; Qt and MSVC runtime modules are loaded from the application directory. Both GUI and offscreen preview plugins are bundled and tested.
- Explorer `.omm` opening, CLI PNG export, native save, overwrite without another picker, crash/relaunch recovery, and explicit Ctrl+W removal from recovery all passed in the installed application.
- The installed executable SHA256 matches the tested stage executable. Actual Windows desktop, recovered-document, and Alt-menu screenshots are included in the verification folder.
- All six shared/macOS regression suites passed (51.81 seconds); the revised animation sampling test also passed separately.

Installed document/recovery acceptance, installer cleanup verification, hashes, logs, and screenshots are recorded in `artifacts/windows/verification`. The test machine is an existing ARM64 VM, not a pristine native x64 PC. The installer bundles the runtime for machines without Qt or Visual Studio; native x64 hardware and a pristine OS remain additional release validation environments.

No remote CI workflow is included.

### Integrated-header follow-up

The Windows header uses QML minimize/maximize/close buttons with standard Windows glyphs. The window keeps its native resize frame but omits the native caption and Qt expanded-caption overlay. Toolbar contents reserve 138 pixels for the controls; empty header space uses system dragging and double-click maximize/restore. Close follows the existing unsaved-document confirmation path.

Final Windows and macOS UI suites passed (84.08 seconds and 42.82 seconds respectively). Windows VM mouse verification passed for dragging, border resizing, double-click maximize, all three controls, and close confirmation. The screenshot and interaction report are in `artifacts/windows-header-vm`. This change is in the source/build binary; the installer verification above refers to the preceding release.
