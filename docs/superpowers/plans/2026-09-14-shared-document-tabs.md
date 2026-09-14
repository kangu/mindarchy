# Shared document tabs: evaluation and implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task by task. Steps use checkboxes for tracking. This document proposes the implementation; it does not authorize starting it.

**Goal:** Place document tabs below the main toolbar, between open sidebars, with consistent interaction on Omarchy, macOS, and Windows.

**Architecture:** Use a shared QML tab strip and a platform-independent document-group model. Each group owns one real application window; each document retains its own engine, editing surface, undo history, viewport, and recovery state. Native APIs continue to provide window controls, menus, dialogs, and platform integration.

**Tech stack:** Existing Qt Quick/QML, C++20, and macOS Objective-C++ integration. No new UI framework.

**Spec:** The evaluation and design contract below are the specification for this plan. Status: proposed for review, September 14, 2026.

## Evaluation

The supplied screenshot shows native window controls, the AppKit tab bar, and the application toolbar occupying separate rows. The app's toolbar is QML content, not an NSToolbar. Consequently AppKit places native tabs above that content.

The current code explains the additional separation:

- `src/macwindow.mm:installMacToolbar` returns early when the native tab bar is visible, before aligning the traffic-light buttons with the 60-point application toolbar.
- `updateMacTabInset` reserves at least 48 points above QML content while the native tab bar is visible.
- `qml/Main.qml` applies that inset to the complete content column.
- Omarchy and Windows already use a 36-point QML strip, but it too precedes the main toolbar and spans the whole window.
- `src/macapplication.cpp` uses native NSWindow groups on macOS and a separate hide/show-window grouping implementation elsewhere.

Apple's public NSWindowTabGroup API exposes ordered windows, selection, membership, overview visibility, and read-only tab-bar visibility. It does not expose a rectangle, content parent, or position for placing the native strip between application sidebars. This was checked against both the documentation and the installed AppKit SDK headers. [NSWindowTabGroup](https://developer.apple.com/documentation/appkit/nswindowtabgroup)

NSTitlebarAccessoryViewController can place an application-owned accessory below the title bar. That changes placement of the accessory, not the existing system tab strip. A native toolbar/accessory redesign could improve the macOS ordering, but would require a separate macOS presentation and would still not provide the requested canvas/sidebar composition directly. [Title-bar accessory layout](https://developer.apple.com/documentation/appkit/nstitlebaraccessoryviewcontroller/layoutattribute)

**Conclusion:** There is no documented direct relocation mechanism for the existing native tab strip that satisfies this layout. Custom application tabs are the recommended solution. This conclusion comes from API and code inspection; no experimental AppKit window was built or moved during this evaluation.

| Approach | Assessment |
| --- | --- |
| Change a native tab position setting | No such public placement setting was found. |
| Rebuild the macOS header with native toolbar/accessory views | Possible architectural alternative for header ordering, but creates a platform-specific UI and does not directly solve alignment between QML sidebars. |
| Move AppKit's internal tab views | Depends on undocumented view structure and OS layout behavior. Exclude from implementation. |
| Shared QML document tabs | Gives direct control of position, dimensions, theme, sidebar alignment, and input across all deployment platforms. Recommended. |

## Design contract

### Layout

Keep the main toolbar at the top of the application content, spanning the window. On macOS, retain the real traffic-light buttons aligned with this toolbar; Windows retains its system-compatible caption controls. Omarchy retains compositor-managed window decoration.

Below the toolbar, arrange three columns:

```text
┌──────────────────────────────────────────────────────────────┐
│ Native window controls + Mindarchy toolbar                    │
├──────────────┬───────────────────────────────┬───────────────┤
│              │ Project A  ×   Project B  ×  +│               │
│ Outline      ├───────────────────────────────┤ Inspector     │
│              │                               │               │
│              │ Mind-map canvas               │               │
│              │                               │               │
└──────────────┴───────────────────────────────┴───────────────┘
```

- The tab strip belongs to the center column and sits immediately above its canvas.
- An open sidebar starts directly below the toolbar and extends alongside both the strip and canvas. Tabs never cover a sidebar or its heading.
- With only the left sidebar open, tabs start at its right boundary. With only the right sidebar open, tabs stop at its left boundary. With both closed, tabs span the available content width.
- Keep the existing 60-point toolbar, 36-point strip, 224-point outline, and 274-point inspector as initial dimensions. Use Qt logical pixels and layout bindings, not screen coordinates.
- Sidebar visibility and widths belong to the window group. Switching documents updates their contents without changing their geometry. Selection, inspector subpage, search, and editing drafts belong to the document.
- Reserve at least 320 logical pixels for the center workspace. At narrow widths, temporarily collapse the inspector first, then the outline; retain the user's requested visibility and restore it when space permits. Never overlap the tabs with a sidebar.
- Show the tab bar only when a window has two or more documents; reclaim its height when one remains. New Tab remains available through the menu and keyboard shortcut.
- Sidebar changes preserve the canvas's world-space center and zoom. Tab switching restores each document's viewport without Fit, recentering, or a camera animation.

### Appearance and interaction

Use existing shell palette tokens. The active tab should visually connect to the canvas; inactive tabs use a quieter background. Indicate selection through shape/border as well as color. Use a clear focus outline, elided long names, and a tooltip with the full title. Explain modified state with an accessible label as well as a dot.

Use tab widths between 120 and 240 logical pixels where space allows. Overflow scrolls horizontally and exposes a list of open documents. Activating, renaming, or reordering a tab keeps the active tab visible. Close and new-tab controls remain usable at narrow widths and high display scaling.

Preserve New Tab, New Window, Close Document, next/previous tab, detach, merge, and session restoration. Add tab drag-reordering and context-menu Move to New Window in the first complete delivery; free-form drag between windows is deferred. Standard modifier conventions remain platform-specific: Command on macOS, Control on Linux/Windows, with physical Control+Tab cycling tabs everywhere.

Tab switching commits valid inline title editing through the existing validation path. A rejected commit cancels the switch. Preserve notes drafts and per-document state; unresolved modal save/date dialogs block conflicting tab commands. Capture draft state before switching or moving a document. Never silently discard an editor or IME composition.

Closing an inactive tab must target that document. If confirmation is necessary, activate it in its existing group and show the appropriate prompt. Cancel keeps the document, tab order, and window intact. Closing the last tab closes that group/window using the existing platform policy; quitting checkpoints all documents before any are destroyed.

### Cross-platform keyboard contract

Use one application-owned shortcut registry for dispatch, menu labels, and the Keyboard Shortcuts reference. Platform differences must be intentional modifier conventions, not divergent implementations. Each keypress invokes exactly one command; avoid registering competing native-menu and QML handlers.

| Action | macOS physical keys | Omarchy / Linux physical keys | Windows physical keys |
| --- | --- | --- | --- |
| New tab in current group | Command+T | Control+T | Control+T |
| New separate window | Command+N | Control+N | Control+N |
| Close active document/tab | Command+W | Control+W | Control+W |
| Next tab, wrapping | Control+Tab | Control+Tab | Control+Tab |
| Previous tab, wrapping | Control+Shift+Tab | Control+Shift+Tab | Control+Shift+Tab |

On macOS, Control in the cycling rows means the physical Control key, not Command. Account explicitly for Qt's platform modifier translation and confirm the actual key events on target hardware. Command+Tab, Alt+Tab, and compositor shortcuts must remain available to the operating system.

When the tab strip has keyboard focus, Left/Right move through tabs, Home/End select the first/last, and Enter/Space activate the focused tab. Selection follows the same editor-validation rules as mouse activation. These local navigation keys must not steal arrow, Home/End, or Space input from the canvas or text editors. Escape returns focus to the active document.

Document shortcuts work while the canvas, outline, inspector, search, or an inline editor has focus, provided no modal dialog is handling the interaction. Resolve valid editor changes before switching; a validation failure keeps the current document and reports the issue. Never route a tab shortcut behind a modal save/close/date dialog. Normal text editing shortcuts remain unchanged.

Move to New Window and Merge All Windows remain keyboard-accessible menu actions using the same dispatch path; do not invent additional global shortcuts or override existing mind-map commands for them.

Required shortcut tests on each platform:

- Three distinctly named tabs: cycle forward/backward through every tab and both wrap boundaries; verify active document identity and unchanged group/native window.
- One tab: cycling is a no-op, without opening a tab or moving keyboard focus to another window.
- Two groups: new/close/cycle affect only the focused group, including after detach, merge, minimize/restore, and fullscreen transitions.
- Focus in each relevant editor/sidebar/search control: trigger new, close, next, and previous; verify exactly one command and preserved drafts/selection/undo.
- Modified document: close prompts for the correct tab; Cancel leaves its content, order, and active state intact. A rejected inline edit vetoes switching.
- Held/repeated cycling: process repeat events safely; next/previous remain responsive without losing events or creating duplicate documents. New/close actions must not repeat destructively from a held key.
- Help text and native/QML menu labels match the actual physical keys on each platform.
- Automated key events are supplemented by physical keyboard checks on macOS, Omarchy, and Windows, including a non-US layout where available. Platform consistency is not established by macOS tests alone.

### Document and window ownership

Do not extend the existing technique of hiding one top-level window and showing another for each tab activation. That makes tab switching a window operation and complicates fullscreen, focus, window placement, and sidebar stability.

Separate document lifetime from window lifetime:

- A document workspace owns its Engine, document identity, recovery record, selection, undo history, and retained QML editing surface.
- A window group owns one ApplicationWindow, an ordered list of workspaces, its active document, window placement, and sidebar geometry.
- Switching tabs selects the retained workspace within that same window. It does not recreate engines, close sessions, or swap native windows.
- Detach moves a workspace into a newly created group; merge moves workspaces into the destination group. Preserve document identity and recovery paths throughout.
- Existing `.omm` documents are unchanged. Migrate `session/tabs.ini` groups by their stored recovery paths and active entry. Keep a backup of prior session metadata until successful restoration; do not restore through native tab-group APIs.
- When old recovery snapshots contain per-document sidebar settings, initialize the new group's sidebar geometry from its active document once. Thereafter persist it at group level.

## Implementation sequence

### 1. Separate document state from the native window

Files: `src/macapplication.cpp`, `src/macapplication.h`, new `src/documentworkspace.h/.cpp`, new `src/windowgroup.h/.cpp`, `src/documentrecovery.h`, `src/viewportstate.h`, and the relevant application tests.

- [ ] Introduce `DocumentWorkspace` and `WindowGroup` ownership as defined above. Keep the existing application's external open/activate/quit entry points while changing their internals.
- [ ] Write failing lifecycle tests before replacing ownership: create two documents in one group, edit independently, switch, and assert unchanged Engine identities, histories, recovery paths, and native window identity.
- [ ] Define the group operations explicitly: `activate(documentId)`, `close(documentId)`, `move(documentId, targetIndex)`, `detach(documentId)`, and `merge(sourceGroupId)`. Each operation either completes or reports a validation/lifecycle failure without partially modifying membership.
- [ ] Refactor recovery and viewport adapters to receive the active document's editing surface independently of the outer window. Preserve existing recovery data and draft semantics.
- [ ] Verify two separate windows still behave independently before introducing custom macOS tabs.

### 2. Extract the shared document surface and tab strip

Files: `qml/Main.qml`, new `qml/DocumentWorkspace.qml`, new `qml/DocumentTabStrip.qml`, `resources.qrc`, `CMakeLists.txt`, and `tests/ui_test.cpp`.

- [ ] Move document-specific canvas/editors into a retained Item-based workspace. Keep global window chrome, toolbar placement, and group sidebar geometry in Main.qml.
- [ ] Give DocumentTabStrip a model of `{id, title, edited}`, an `activeDocumentId`, and signals `activateRequested(id)`, `closeRequested(id)`, `newRequested()`, and `moveRequested(id, targetIndex)`.
- [ ] Place the strip and workspace in the center column after the main toolbar. Bind both sidebar widths and visibility to the group geometry contract.
- [ ] Implement active/hover/focus/modified states, horizontal overflow, a document list, drag-reordering, and keyboard-accessible close/new controls using shared palette tokens.
- [ ] Add geometry tests for no sidebar, left only, right only, and both. Assert toolbar bottom equals strip top, strip boundaries equal canvas boundaries, and no sidebar intersects the strip.
- [ ] Test the 600-point minimum window width, long titles, one/two/many tabs, and temporary responsive sidebar collapse.

### 3. Route all platform tab commands through the shared model

Files: `src/macwindow.mm`, `src/macapplication.cpp`, `qml/DesktopMenus.qml`, `qml/KeyboardShortcuts.qml`, `qml/Main.qml`, `tests/macapplication_test.mm`, and `tests/application_test.cpp`.

- [ ] Disable native macOS document tabbing with `NSWindowTabbingModeDisallowed` and retain automatic-window-tabbing opt-out.
- [ ] Remove native tab grouping, selection, inset calculations, and the AppKit newWindowForTab responder override once all commands use the shared group model.
- [ ] Replace native next/previous/detach/merge menu actions with app-owned dispatch. Remove Show Tab Bar because the shared strip remains visible.
- [ ] Implement the cross-platform keyboard contract through the shared shortcut registry. Test modifier resolution, one-command dispatch, wraparound, focus contexts, modal blocking, and key repeat on each platform.
- [ ] Keep native traffic-light buttons, native menu integration, file dialogs, fullscreen, minimize, and restore behavior. Do not replace system buttons with drawn imitations.
- [ ] Replace native-tab-membership assertions with one-native-window-per-group assertions. Verify changing tabs never leaves fullscreen, flashes another window, or changes the window rectangle.

### 4. Preserve editing, close, detach, merge, and restart behavior

Files: shared workspace/group classes, `src/documentsession.h`, `src/documentrecovery.h`, application integration tests, and platform-specific dialog tests.

- [ ] Make tab activation respect the boolean result of editor validation. The existing invocation of commitForTabSwitch does not consume its return value; carry the result through the new dispatch path.
- [ ] Test uncommitted title/notes/date drafts, rejected text, active IME composition, and native save dialogs while switching or closing tabs.
- [ ] Verify closing an inactive dirty document prompts for that document and Cancel preserves all work.
- [ ] Verify detach and merge retain order, active document, selection, viewport, undo, and recovery without serializing/reopening document content.
- [ ] Read the prior tabs.ini format into shared groups and write the new group settings atomically. Test restart with missing/unreadable files and unsaved recovered documents.
- [ ] Exercise coordinated quit failure: one failed checkpoint leaves all documents open and recoverable.

### 5. Verify platform behavior and update documentation

Files: `tests/ui_test.cpp`, `tests/macapplication_test.mm`, `tests/application_test.cpp`, `tests/macwindow_test.mm`, `README.md`, and `docs/controls.md`.

- [ ] Run the engine, canvas, UI, recovery/application, and window-placement suites after building. Use `cmake --build build-macos --parallel` followed by `ctest --test-dir build-macos --output-on-failure` on the existing macOS build; use each target platform's configured build directory elsewhere.
- [ ] Capture the actual app on macOS, Omarchy/Wayland, and Windows with each sidebar combination, long/overflowing tabs, and high display scaling. Use synthetic maps.
- [ ] On real platform desktops, verify window dragging, maximize/fullscreen transitions, tab switching, focus, keyboard access, detach/merge, close/cancel, and restart. Compilation alone does not establish platform parity.
- [ ] Verify tab roles, selection state, names, modified markers, close-button labels, and keyboard focus with the platform accessibility tools.
- [ ] Update the README's native-macOS-tabs description and the shortcut reference to match shared document tabs.

## Acceptance criteria

The visible result must have one stable toolbar with correctly placed native window controls, followed by a shared tab strip occupying only the canvas column. Open sidebars retain their width and height when switching documents. One, two, or twenty documents do not introduce an extra native title-bar/tab row.

Each document retains its own content, editing drafts, undo history, viewport, and recovery state. Tab cycling stays in the same native window and fullscreen session. Existing document files require no migration. Existing saved tab groups reopen as shared groups. The result is verified on all three deployment platforms before being described as consistent across them.

All five shortcuts in the keyboard contract must pass on every target platform with the stated physical modifiers. Menu actions, pointer actions, and shortcuts must produce the same document/group result and respect the same validation and close-confirmation rules.

## Delivery boundary

This is a coordinated window/document UI change, not merely a change to the tab strip's vertical offset. Implement and validate it as a focused branch. Avoid unrelated editor, file-format, theme, or backend changes. The existing worktree and README edits must be preserved.

## Implementation outcome — 2026-09-14

Implemented the shared host/workspace model, common QML tab strip, responsive group sidebars, command registry, targeted close/detach, ordered merge/recovery, and native tab removal. Retained the existing `MacApplication` class name to avoid unrelated entry-point churn; it manages all desktop platforms. Native device verification is complete on macOS. Omarchy needs Tailscale reauthentication, and Windows runtime verification remains outstanding. See `docs/progress/2026-09-14T15-49-44-shared-document-tabs.md` for validation and delivery details.
