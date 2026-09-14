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

Open **Inspector → Theme** to apply a palette. The new collection—**Porcelain**, **Sky**, **Starlight**, **Sage**, **Blush**, and **Graphite**—appears first, followed by every existing theme. New documents start with **Omarchy** on detected Omarchy installations, with **Porcelain** second in the picker. Other systems default to **Porcelain**, with **Omarchy** second; existing documents retain their chosen theme. Each new theme controls task completion color, checkbox corners and weight, and progress-ring styling. Theme changes preserve authored text formatting, support undo/redo and persist with the document. See [the collection notes](docs/refined-theme-collection.md) and [sample maps](examples/refined-themes).

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
- Tasks, completion, notes and cross-branch relationships (select two nodes, then Cmd+L / Ctrl+L).
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

Titles/notes are bounded to 16,384 characters and imports to 20 MB. Oversized title geometry is rejected before replacing the document. Relationships are simple untitled connectors. Left/right balanced maps, images/attachments, custom theme editing/import, MindNode file interchange, SQLite, collaboration, screen-reader parity, native printing and production installers remain outside this prototype. Trackpad/IME/device behavior requires human testing on target hardware.

## Product documents

- [MindNode interaction reference](../docs/mindnode-classic-interactions-2026-09-07.md), with trackpad evidence and theme experiments.
- [Interaction PRD](../docs/mindmap-interaction-prd.md).
- [Gesture implementation plan](../docs/superpowers/plans/2026-09-07-mindmap-interactions.md). This plan is not yet implemented; themes were authorized and delivered separately.

Toolbar actions now use local [Lucide icons from Iconify](https://icon-sets.iconify.design/lucide/) with text tooltips and accessible names. The sample controls and stats footer are removed. Icon sources and license are in `qml/icons/`.

## Application icon

The application uses `assets/mindarchy-icon.png`, derived from the primary logo in `assets/minarchy-logo-concept.png`, on every supported platform:

- macOS: `build-macos/mindarchy.app`, with an ICNS resource and bundle identifier `org.mindarchy.app`. Open the bundle in Finder or use `./run.sh`.
- Windows: multi-resolution ICO embedded through CMake RC or qmake RC_ICONS. The Windows installer is built and tested in a Windows 11 ARM64 VM using x64 emulation.
- Linux/Omarchy: Qt runtime icon and a matching `org.mindarchy.app.desktop` launcher with hicolor PNG sizes. For this development checkout, run `python3 packaging/install-linux-icon.py`; CMake install also installs the launcher/icons.

The toolbar uses the same artwork. Native icon assets are committed-source build inputs; regenerating them on macOS uses `python3 packaging/generate-icons.py` (sips/iconutil and Swift/AppKit). The macOS icon uses 85% artwork size with transparent margins, including the runtime Dock icon. No icon-generation tools are needed to build on Linux or Windows.


## Windows installer releases

Build locally with PowerShell and the installed Inno Setup compiler; no GitHub remote or CI is needed. Run `scripts/release-windows.ps1 -QtDir C:\Qt\6.11.2\msvc2022_64` to build, test, deploy Qt and compile the per-user EXE installer. See [Windows prerequisites, commands and platform behavior](docs/platforms/windows.md).

## macOS installer releases

Run `python3 scripts/release-macos.py --version 0.1.0 --unsigned` to build, test and package a self-contained installer for `/Applications/Mindarchy.app`. Signed/notarized releases, architecture choices, installation and verification details are in [macOS release workflow](docs/macos-release.md).

Document format, Finder previews, and Linux associations: [OMM documents](docs/omm-documents.md).

## Startup documents

Normal startup reopens the saved documents from the previous session, each in its own window. Missing, unreadable, or invalid documents are skipped. With nothing to restore, the app starts with one blank, editable central node. Explicit file opens and New bypass session restoration; launching while other windows are running does not duplicate them. On macOS, recovery snapshots also reopen unsaved documents and drafts (see below). On other platforms, unsaved changes still use the Save/Discard/Cancel close flow.

Session metadata lives in the platform application-data directory under `session/documents.ini`. Per-window locks track each document and recover stale entries after a crash, including multiple macOS windows in one process. Screenshot, benchmark, and timed test runs do not read or modify the saved session.

Cmd+W closes the active window after any save confirmation and removes it from session restoration. On macOS, Cmd+Q atomically checkpoints every window to recovery storage, without a Save dialog. All windows stay open until every checkpoint succeeds; a failed write cancels quitting and displays an error. Relaunching restores those windows, original filenames, unsaved status, unfinished node text/notes/date entries, viewport and per-window placement. Unavailable displays use the existing placement fallback.

Recovery copies live at `~/Library/Application Support/Mindarchy/session/<window-id>.recovery`, with owner-only file permissions. They are private JSON envelopes containing document JSON, the last saved baseline, original path and UI drafts; they do not change the user's `.omm` file. These are durable Application Support files rather than purgeable temporary files. Each active macOS window checks for changes every second and writes only changed snapshots. Atomic replacement retains the previous complete snapshot on write failure. Crashes recover the latest completed snapshot; changes within the last second (or while the UI thread/storage is blocked) may not yet be captured. Uncommitted IME composition is committed when quitting, but a crash can interrupt composition. Explicitly closing/discarding a window removes its recovery copy. Invalid recovery files are retained on disk and reported rather than deleted. Recovery storage is local to this computer, not a substitute for backups.

On Omarchy, the existing coordinated Save/Discard/Cancel quit flow remains in use.

## Mindarchy rename compatibility

The application, bundle (`org.mindarchy.app`), Linux launchers, document type (`org.mindarchy.omm`), artwork and release packages use Mindarchy. Documents use the `mindarchy` v1 format. This pre-release build starts with the Mindarchy storage namespace and does not migrate older prototypes or support their document-format identifiers. Historical progress records retain their original names.

### Live Omarchy shell theme

On Linux, Mindarchy reads `$XDG_STATE_HOME/omarchy/current/theme/colors.toml` (normally `~/.local/state/omarchy/current/theme/colors.toml`) on current Omarchy releases, falling back to the older `$XDG_CONFIG_HOME/omarchy/current/theme/colors.toml` location when the state directory is absent. Background, foreground, accent, and error colors drive the application shell, including panels, controls, icons, dialogs, and tooltips. Document themes, preview thumbnails, color presets, and viewport settings remain independent.

Filesystem watches apply valid palette changes live, with a one-second recovery check for replaced directories or symlinks. Missing or invalid initial palettes use the built-in shell; an interrupted theme switch keeps the last valid palette until the replacement is ready. No Omarchy hooks, restart, or document writes are required. Automatic detection is Linux-only.

Both the current named `red` color and older `color1` palette key are supported. The supported palette format and replacement behavior follow [Omarchy's theme setter](https://github.com/basecamp/omarchy/blob/master/bin/omarchy-theme-set).


### Branch clipboard and Focus mode

With the canvas focused, **Cmd+C / Cmd+V** copies selected subtrees and pastes them beneath the selected node (Ctrl on other systems). Normal text copy/paste remains available inside node titles, notes, and other text fields.

Branch clipboard data preserves rich titles, notes, task/date/meeting data, folded state, resolved styling and relationships whose endpoints are both copied. Overlapping selections are copied once. IDs are reassigned on paste, insertion is one undo step, and the destination theme remains unchanged. New branches receive destination layout placement; source manual offsets are reset. Connections to uncopied nodes are omitted. Clipboard imports use the same size, depth, geometry and graph validation as documents. Indented plain text and Markdown bullet/numbered lists can also be pasted as child hierarchies; copying branches supplies an indented plain-text fallback to other applications.

Use **Cmd+Shift+F** (Ctrl+Shift+F elsewhere) to spotlight the selected subtree and its ancestors. Focus eases into a roughly 2.2× closer view of the selected node over 650 ms, bounded by its size and the zoom limit. The breadcrumb navigates up the hierarchy. **Escape** or **Exit Focus** animates back to the prior pan and zoom, without changing folds or document content. Manual pan, zoom, or editing interrupts the camera animation. Focus is temporary: persisted viewport and recovery state use the view from before entering Focus, including during the return animation. Clicking dimmed unrelated nodes is suppressed; keyboard navigation or search outside the focused area exits Focus.

### Links & Resources

Select one node and open **Inspector → Node → Links & Resources**. **Add link** accepts HTTP(S) addresses (a bare hostname gets `https://`); **Add file** links a local file using Browse or a typed path. Each resource has an optional name and explicit Open, Edit, and Remove actions. Open uses the system's default browser or file application. Missing files report an error and can be relinked with Edit.

Resources are references, not embedded attachments. They are stored in the regular `.omm` JSON, with file paths relative to the document folder when possible. Move the referenced files alongside the map to retain portability. Save As and cross-document branch paste preserve the resolved targets. Unsaved maps use absolute file references until saved; typed relative paths require a saved map. Add/edit/remove support undo, and resource data is included in recovery. Each node supports up to 100 resources; web addresses are restricted to HTTP(S). No resources open automatically when loading a map.

With one text or task node selected and the canvas focused, typing starts title editing and replaces the previous title. The first character is retained, further typing continues normally, and committing is a single undoable title change. Cmd+Return/F2 still opens the existing title for editing. Space remains reserved for canvas panning. Fold/expand and Task shortcuts are now Alt+F and Alt+T; bare +, −, and 0 zoom controls apply only with no node selected.

Trackpad scroll gestures pan on both axes and stay in pan mode through angle-only updates and momentum. Pressing Control during an active pan does not switch it to zoom. Explicit scroll-end events release the pan lock; backends without gesture phases release it after 250 ms of inactivity. Ordinary mouse-wheel zoom remains available outside an active pan gesture.

### Application menu on Linux

The top-right hamburger opens **File → Window → Help** submenus. File and Help use the same menu components as Windows; Window includes minimize, maximize/restore, full screen, next/previous window, Bring All to Front, and a live document list from the session registry. Help opens the shared Keyboard Shortcuts window. Closing that window restores focus to the document. Center is disabled on Linux because the Wayland compositor controls window placement; activation and other window operations remain subject to compositor policy. The macOS native menus and Windows Alt menu entry point are retained.

Branch copy/paste, Focus, and Connect are keyboard actions rather than toolbar buttons: Cmd+C / Cmd+V, Cmd+Shift+F, and Cmd+L respectively (Ctrl instead of Cmd on Linux/Windows). Connect requires exactly two selected nodes.

### File menu and recent documents

Use **File → New / Open / Save** in the native macOS menu, the Windows Alt menu, or the Omarchy hamburger menu. **Open Recent** remembers the last 15 successfully opened or saved documents across windows and app restarts. Missing files remain visible but disabled; **Clear Menu** clears this history without changing startup window restoration. Opening a file from the dialog or recent list uses a separate document window, preserving work in the current document. On macOS, reopening an already open file activates its existing window.

### macOS application windows and Dock

Interactive macOS documents share one application process, Dock icon, and App Exposé group. Right-click the Dock icon to select any open mind map from the standard native document list. File commands target the active document; New, Finder file opens, recent files, and subsequent command-line launches are routed into this application. Window cycling uses document identities rather than process IDs. Closing the last window leaves the application running; clicking its Dock icon creates a new blank window. Cmd+W forgets the closed document for startup restoration, while Cmd+Q preserves all remaining windows and unsaved drafts. Windows and Omarchy keep their existing process model.

The native window registration supplies the Dock list without adding duplicate custom entries, following [Apple’s Dock guidance](https://developer.apple.com/library/archive/documentation/General/Conceptual/MOSXAppProgrammingGuide/CommonAppBehaviors/CommonAppBehaviors.html).

### Dragging selected branches

Drag a node onto another node to re-parent its entire subtree in automatic or manual placement. Select multiple nodes (modifier-click or marquee), then drag any selected node to move all selected branches together. Selected descendants of another selected node travel with that ancestor once. Tree order is preserved and the whole drop is one undo step; drops into a moving branch or its descendants are rejected.

Automatic placement also supports sibling insertion targets. In manual placement, dropping onto a parent places the moved branches beside it while retaining each subtree’s relative geometry and keeping unrelated nodes in place. Dropping on empty canvas moves the selection freely, including the existing mirrored-branch behavior.

### Images on nodes

Drag a local image file onto a node to attach it on the left of its content. A highlighted outline identifies the drop target. In the Node inspector, use Image Placement to choose Left, Right, Top, or Bottom. Placement is saved with the document and retained when replacing the image. Click the image to reveal resize handles; drag an edge or corner to resize proportionally, or press Escape to cancel. Each completed resize is one undo step. Images work with text, tasks, and calendar nodes in both placement modes.

Right-click an image for Preview, Replace Image, Cut Image, Copy Image, Paste Image, Reset Image Size, and Remove Image. With an image selected, Cmd+C / Cmd+X (Ctrl+C / Ctrl+X on Linux and Windows) copy or cut the image. Select a destination node and press Cmd+V / Ctrl+V to paste, replacing its existing image if present. Transfers preserve size and placement and support undo; regular image clipboard data from other apps is accepted too. Double-click or press Space while the image is selected to open the preview window. On Omarchy, preview is a borderless in-app image overlay, dismissed with Space, Escape, or an outside click. On macOS, preview uses a rounded floating panel without a title bar or window buttons; clicking back on the document dismisses it. Space or Escape closes it and returns keyboard focus to the canvas. Space-drag still pans when no image is selected. Delete or Backspace removes a selected image, leaving its node and children intact. Undo restores the image. Holding the key does not continue into deleting the node.

Images are embedded in the regular JSON `.omm` document, so they travel between macOS and Omarchy without their original source files. Save/reopen, recovery, branch copy/paste, and preview rendering preserve them. Imports are downsampled to a maximum of 2,048 pixels on the longest side without enlarging small originals. Transparent images remain PNG. Opaque images use JPEG at quality 88 when it is at least 20% smaller than PNG; otherwise PNG is retained. The original source file is never modified. Animated inputs use their first decoded frame.

The default displayed longest side is at most 120 logical pixels. Resizing changes display dimensions, not the stored pixels. Imports are limited to 64 MB/64 megapixels; embedded images collectively have a 12 MB compressed and 128 MB decoded budget within the existing 20 MB document limit.

## One-command installer builds

From `qt-prototype`, run any of:

```bash
VERSION=0.1.0 ./scripts/build_macos.sh
VERSION=0.1.0 ./scripts/build_omarchy.sh
WINDOWS_QT_DIR='C:\Qt\6.11.2\msvc2022_64' ./scripts/build_windows.sh
./scripts/build_all.sh
```

macOS uses the existing build/test/DMG+PKG workflow, unsigned by default. Set `MACOS_SIGN=1` for signing with the existing release credentials; extra arguments such as `--qt` and `--arch` pass to the release tool. Output: `dist/macos/`.

Omarchy uses `makepkg` to produce a native `.pkg.tar.zst` package in `artifacts/omarchy/`. Run as a normal user with `base-devel`, `qt6-base`, `qt6-declarative`, and `qt6-wayland` installed. The package declares its Qt dependencies and installs the executable, desktop launcher, MIME association, thumbnailer, and icon. Install it with `sudo pacman -U <package>`. This build does not run the GUI test suite.

Windows uses the existing PowerShell/MSVC/Qt/Inno Setup build and test workflow. Run the shell wrapper in Git Bash, or invoke `scripts/release-windows.ps1` directly from PowerShell. Output: `artifacts/windows/`. See `docs/platforms/windows.md` for prerequisites.

To orchestrate all three from macOS, configure SSH build hosts:

```bash
export VERSION=0.1.0
export OMARCHY_HOST=user@omarchy-host
export OMARCHY_SOURCE=/home/user/mindmap-qt-lab
export WINDOWS_HOST='your-user@your-windows-host'
export WINDOWS_SOURCE='C:\Projects\mindmap-blue\qt-prototype'
export WINDOWS_QT_DIR='C:\Qt\6.11.2\msvc2022_64'
./scripts/build_all.sh
```

Each remote machine must already have the **same current source checkout** and its build prerequisites. The scripts do not sync source, install dependencies, start VMs, or use CI. Windows must expose SSH and PowerShell. Artifacts stay on the machine that builds them. `build_all.sh` runs all three sequentially, reports each result, and returns a nonzero exit status if any build fails. Missing host configuration is reported as a failure, never silently skipped.

## Document tabs and windows

- Cmd+T on macOS / Ctrl+T elsewhere creates a new tab in the current window.
- Cmd+N / Ctrl+N creates a separate window.
- Physical Control+Tab cycles forward through the current window's tabs; Control+Shift+Tab cycles backward, wrapping at the ends on every platform.
- Window → Move Tab to New Window detaches the current tab. Window → Merge All Windows groups the open documents.
- Close affects the current document and uses its existing save/discard behavior. Quit preserves recovery snapshots and tab groups for startup.

macOS uses AppKit's tab bar, including the native plus/close buttons and tab dragging. Omarchy and Windows use a matching scrollable strip with document titles, edited markers, close buttons, and a plus button. The shared application manager keeps all documents in one process; on these platforms only the active document window in each tab group is shown. The Omarchy strip and menu follow the live shell palette. Each document retains its own canvas, undo history, file path, viewport, and recovery snapshot. Group membership is stored outside `.omm` files in the session directory's `tabs.ini`.

After updating from the earlier separate-process Omarchy/Windows build, fully quit existing instances before launching the new executable. Windows uses the shared implementation, but this tab feature has not yet been runtime-tested on Windows.

Each of the 19 map themes has a distinct bundled open-source base font, including Inter, Manrope, Lora and Newsreader. Explicit node font overrides are preserved. Font sources and SIL licenses are included under `assets/fonts`; see the [theme collection](docs/refined-theme-collection.md) for the full pairing table.

Automatic layout keeps node and connection motion synchronized when adding child/sibling nodes or committing a title. Inline editing follows the moving node without cancelling the reflow animation.

Omarchy is the second theme in the picker: a dark Tokyo Night-inspired palette with bundled JetBrains Mono, terminal-green task indicators and restrained node corners.

Platform defaults are detected at runtime on Linux using OMARCHY_PATH, the standard system/user Omarchy installation directories, or current Omarchy theme state under XDG_STATE_HOME/XDG_CONFIG_HOME. Saved document themes are never overwritten.
