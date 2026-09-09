# Mindarchy

A Qt Quick + C++20 prototype for testing a MindNode-like editor on Omarchy. It is isolated from the existing Vue/Tauri application.

## Run on the Omarchy machine

The source and compiled application are at `/home/user/mindmap-qt-lab` on `omarchy-host` (SSH user `user`). In an Omarchy terminal:

```bash
cd ~/mindmap-qt-lab
./run.sh
```

For an SSH launch into the existing desktop session:

```bash
ssh user@omarchy-host 'env XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland /home/user/mindmap-qt-lab/run.sh'
```

These display values are the ones verified in this session. Qt 6.11.2, its Wayland plugin, qmake6, GCC and make were already installed; no system packages were installed.

## Watch real GUI tests

```bash
cd ~/mindmap-qt-lab
./scripts/live-tests.sh
```

This opens its own sample window, labels the current test in the title and pauses between actions. It does not edit documents in another running prototype window. Set `MINDMAP_LIVE_TEST_MS=3000` for longer pauses. The tests exercise the real QML interface and C++ event handlers. They synthesize Qt input events; they do not validate physical device drivers or IME candidate windows.

## Themes

Open **Inspector → Themes** and click a preview to apply **Beach Day**, **Holographic**, **Retro** or **Arcade**. Tab to a card and press Space for keyboard activation. Lab restores the legacy palette. Theme changes preserve your map, can be undone/redone and are saved in JSON. Pending title and notes drafts are protected.

The presets reproduce the observed canvas palettes and node styles by depth using original previews and portable typography. They are visual approximations, not exact MindNode theme assets or font metrics. Explicit rich-text colors remain explicit and may need adjustment after switching between light and dark themes.

To launch directly into the panel on this Mac:

```bash
./run.sh --theme beach-day
```

On Omarchy use `./run.sh --theme beach-day`.

## Included

- Persistent window placement on macOS and Omarchy, with disconnected-display fallback; floating Hyprland windows restore through the compositor. macOS zoom/maximize is prepared on the hidden native window before it appears, avoiding a normal-size flash or visible zoom animation and preserving the normal rectangle for restore-down. The placement suite runs against real AppKit on macOS to cover title-bar zoom and repeated reopen cycles.

- Date nodes with Monday-first week/month calendars, period navigation, themed assigned days and hover-only entry previews.

- Four MindNode-inspired theme presets with depth-based styling, previews, undo and persistence.
- Node inspector with eight shapes, fixed-width wrapping, custom colors/strokes, font controls, alignment, bulk styling and reset to theme.
- Horizontal, Vertical and Compact placement with geometry-aware arrow navigation.
- Automatic/manual placement, spacing presets, rounded/angular connectors.
- Selection, additive multiselection and marquee selection.
- Keyboard child/sibling creation, editing, folding and deletion.
- Rich-text title editing with bold, italic, underline and multiline text.
- Inline editing uses the node's actual themed shape, text metrics and zoom. Only the draft node resizes while typing; layout runs on commit and keeps that node anchored on screen.
- Pointer pan/zoom, cursor-anchored zoom, fit, drag preview, reparenting and sibling insertion.
- Manual subtree movement with undo/redo.
- Tasks, completion, notes and cross-branch relationships (select two nodes, then Connect).
- An outline and an inspector that collapse for narrow Hyprland tiles.
- Atomic versioned JSON save/open with validation; PNG export of the current canvas viewport.
- Demo, 1,000-node and 10,000-node command-line fixtures and benchmark measurements.

See [controls](docs/controls.md) for the interaction reference. Normal edits are not autosaved. Large test fixtures are available using `--nodes 1000` or `--nodes 10000` at launch.

## Build

Qt 6.6+ development packages with Core, Gui, Qml, Quick, QuickControls2 and Test; C++20 compiler. Omarchy validation used Qt 6.11.2.

```bash
mkdir -p build
cd build
qmake6 ../prototype.pro
make -j2
./mindarchy
```

CMake alternative:

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel 2
ctest --test-dir build-cmake --output-on-failure
```

The macOS CMake build is verified with Qt 6.11.2 installed at `~/Qt/6.11.2/macos`; all three test suites pass and the application runs on Cocoa/Metal. The older Homebrew Qt remains broken, so select the new installation explicitly:

```bash
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.2/macos"
cmake --build build-macos --parallel 4
./run.sh
```

Windows x64 builds with MSVC 2022 and Qt 6.11.2. The local Inno Setup workflow produces a self-contained per-user installer; see the [local Windows build instructions](docs/platforms/windows.md).

## Tests and measurements

```bash
./scripts/test.sh
QT_QPA_PLATFORM=offscreen ./build/mindarchy --benchmark
./build/mindarchy --nodes 10000 --render-benchmark
```

The first command runs engine, canvas and complete-QML tests. The second prints JSON for cold fixture creation and layout switching. The third opens the complete app and measures a scripted pan after warm-up, then exits. Run performance measurements without concurrent compilation or other test windows.

`Layout` is synchronous CPU layout time. `Scene build` is the CPU time spent updating the scene graph; it is not GPU execution time, FPS or end-to-end input latency. The render benchmark additionally records compositor-paced frame intervals. At overview zoom, labels are omitted. Panning inside the retained overscan region updates only the scene transform; crossing that region rebuilds visible geometry. Results therefore depend on viewport, zoom and content.

Measured results and test limitations are recorded in [verification](docs/verification.md).

## Architecture

- `src/engine.*`: indexed ordered tree, validated documents, selection, layout, cached measurements, history and JSON persistence.
- `src/canvas.*`: Qt Quick scene graph geometry, retained textures, culling, pointer/keyboard interaction, editor targeting and PNG capture.
- `qml/Main.qml`: application shell, outline, inspector and a single active rich-text editor.
- `src/main.cpp`: startup and benchmark/screenshot entry points.
- `tests/`: independent engine tests, canvas input tests and complete-QML interaction tests.

GPU rendering uses Qt's backend abstraction. Native Wayland/OpenGL was verified on the Omarchy host. A simplified software fallback permits headless event tests; it does not reproduce every connector/style feature and is not the performance target.

## Prototype boundaries

This is a test vehicle, not complete MindNode parity. It has one root, a 10,000-node limit and maximum depth 512. Layout still recomputes the visible tree, although text measurements are cached. Undo uses bounded copy-on-write snapshots (40 entries / 200,000 retained node records), not a final delta-command implementation. Inactive text uses cached raster labels; a final editor should evaluate scalable text rendering. Raster budgets may omit labels in extreme scenes.

Titles/notes are bounded to 16,384 characters and imports to 20 MB. Oversized title geometry is rejected before replacing the document. Relationships are simple untitled connectors. Left/right balanced maps, images/attachments, custom theme editing/import, MindNode file interchange, clipboard branch interchange, SQLite, collaboration, screen-reader parity, native printing and production installers remain outside this prototype. Trackpad/IME/device behavior requires human testing on target hardware.

## Product documents

- [MindNode interaction reference](../docs/mindnode-classic-interactions-2026-09-07.md), with trackpad evidence and theme experiments.
- [Interaction PRD](../docs/mindmap-interaction-prd.md).
- [Gesture implementation plan](../docs/superpowers/plans/2026-09-07-mindmap-interactions.md). This plan is not yet implemented; themes were authorized and delivered separately.

Toolbar actions now use local [Lucide icons from Iconify](https://icon-sets.iconify.design/lucide/) with text tooltips and accessible names. The sample controls and stats footer are removed. Icon sources and license are in `qml/icons/`.

## Application icon

The application uses `assets/mindarchy-icon.png`, derived from the primary logo in `assets/minarchy-logo-concept.png`, on every supported platform:

- macOS: `build-macos/mindarchy.app`, with an ICNS resource and bundle identifier `blue.mindmap.lab`. Open the bundle in Finder or use `./run.sh`.
- Windows: multi-resolution ICO embedded through CMake RC or qmake RC_ICONS. The Windows installer is built and tested in a Windows 11 ARM64 VM using x64 emulation.
- Linux/Omarchy: Qt runtime icon and a matching `blue.mindmap.lab.desktop` launcher with hicolor PNG sizes. For this development checkout, run `python3 packaging/install-linux-icon.py`; CMake install also installs the launcher/icons.

The toolbar uses the same artwork. Native icon assets are committed-source build inputs; regenerating them on macOS uses `python3 packaging/generate-icons.py` (sips/iconutil and Swift/AppKit). The macOS icon uses 85% artwork size with transparent margins, including the runtime Dock icon. No icon-generation tools are needed to build on Linux or Windows.


## Windows installer releases

Build locally with PowerShell and the installed Inno Setup compiler; no GitHub remote or CI is needed. Run `scripts/release-windows.ps1 -QtDir C:\Qt\6.11.2\msvc2022_64` to build, test, deploy Qt and compile the per-user EXE installer. See [Windows prerequisites, commands and platform behavior](docs/platforms/windows.md).

## macOS installer releases

Run `python3 scripts/release-macos.py --version 0.1.0 --unsigned` to build, test and package a self-contained installer for `/Applications/Mindarchy.app`. Signed/notarized releases, architecture choices, installation and verification details are in [macOS release workflow](docs/macos-release.md).

Document format, Finder previews, and Linux associations: [OMM documents](docs/omm-documents.md).

## Startup documents

Normal startup reopens the saved documents from the previous session, each in its own window. Missing, unreadable, or invalid documents are skipped. With nothing to restore, the app starts with one blank, editable central node. Explicit file opens and New bypass session restoration; launching while other windows are running does not duplicate them. On macOS, recovery snapshots also reopen unsaved documents and drafts (see below). On other platforms, unsaved changes still use the Save/Discard/Cancel close flow.

Session metadata lives in the platform application-data directory under `session/documents.ini`. Per-window locks coordinate separate processes and recover stale entries after a crash. Screenshot, benchmark, and timed test runs do not read or modify the saved session.

Cmd+W closes the active window after any save confirmation and removes it from session restoration. On macOS, Cmd+Q atomically checkpoints every window to recovery storage, without a Save dialog. All windows stay open until every checkpoint succeeds; a failed write cancels quitting and displays an error. Relaunching restores those windows, original filenames, unsaved status, unfinished node text/notes/date entries, viewport and per-window placement. Unavailable displays use the existing placement fallback.

Recovery copies live at `~/Library/Application Support/MindmapBlue/Mindmap Lab/session/<window-id>.recovery`, with owner-only file permissions. They are private JSON envelopes containing document JSON, the last saved baseline, original path and UI drafts; they do not change the user's `.omm` file. The historical directory name is intentional. These are durable Application Support files rather than purgeable temporary files. Each active macOS window checks for changes every second and writes only changed snapshots. Atomic replacement retains the previous complete snapshot on write failure. Crashes recover the latest completed snapshot; changes within the last second (or while the UI thread/storage is blocked) may not yet be captured. Uncommitted IME composition is committed when quitting, but a crash can interrupt composition. Explicitly closing/discarding a window removes its recovery copy. Invalid recovery files are retained on disk and reported rather than deleted. Recovery storage is local to this computer, not a substitute for backups.

On Omarchy, the existing coordinated Save/Discard/Cancel quit flow remains in use.

## Mindarchy rename compatibility

The application, executable, artwork resources and release packages now use Mindarchy. The repository directory is unchanged. The internal `blue.mindmap.lab` application/launcher identity, `blue.mindmap.omm` document type, and `mindmap-lab` v1 JSON format remain stable so existing file associations and documents continue to work. Window preferences and the session registry retain their historical storage namespace through `src/appidentity.h`; existing window geometry and reopened documents are preserved. Archived release artifacts retain their original names; the release workflow produces new `Mindarchy` packages.

### Live Omarchy shell theme

On Linux, Mindarchy reads `$XDG_STATE_HOME/omarchy/current/theme/colors.toml` (normally `~/.local/state/omarchy/current/theme/colors.toml`) on current Omarchy releases, falling back to the older `$XDG_CONFIG_HOME/omarchy/current/theme/colors.toml` location when the state directory is absent. Background, foreground, accent, and error colors drive the application shell, including panels, controls, icons, dialogs, and tooltips. Document themes, preview thumbnails, color presets, and viewport settings remain independent.

Filesystem watches apply valid palette changes live, with a one-second recovery check for replaced directories or symlinks. Missing or invalid initial palettes use the built-in shell; an interrupted theme switch keeps the last valid palette until the replacement is ready. No Omarchy hooks, restart, or document writes are required. Automatic detection is Linux-only.

Both the current named `red` color and older `color1` palette key are supported. The supported palette format and replacement behavior follow [Omarchy's theme setter](https://github.com/basecamp/omarchy/blob/master/bin/omarchy-theme-set).
