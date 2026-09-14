# Shared document tabs

## Request
Move tabs below the application toolbar, account for either sidebar, and provide consistent tab shortcuts across Omarchy/Linux, macOS, and Windows.

## Implementation
`Main.qml` now owns the real application window. Each document retains a `DocumentWorkspace.qml` surface, engine, canvas, undo history, draft editors, viewport, and recovery session. Activating a tab changes the visible workspace without replacing or hiding the host window. The themed 36-pixel tab strip occupies the canvas column below the 60-pixel toolbar. Sidebar preferences belong to the group; narrow windows collapse the inspector first and restore requested panels as space returns.

Tabs provide edited indicators, close and new buttons, active-tab reveal, horizontal overflow and a document list, keyboard focus navigation, drag reorder, and targeted detach. Merge preserves group order. Closing an inactive document prompts for that document and cancellation returns to the previous map. Native macOS tabbing is disabled while retaining native window controls and application menus.

`TabShortcuts` supplies dispatch and help metadata, including native macOS menu accelerators. Command/Ctrl+T opens a tab, Command/Ctrl+N opens a window, and Command/Ctrl+W closes the current tab. Physical Control+Tab and Control+Shift+Tab cycle on every platform, accounting for Qt’s macOS modifier mapping. Invalid inline edits veto tab operations, notes drafts remain intact, modal dialogs block tab commands, and new/close commands suppress repeat events.

Session metadata preserves group order, selected tabs, the active group, and sidebar state. Previous tab metadata is backed up during migration. Each live group uses separate placement storage copied into its documents’ recovery placement records. The `.omm` format is unchanged. Coordinated quit retains the existing checkpoint-before-close behavior, including failure cancellation and native-dialog guards.

## Verification
The worktree build passed the native macOS lifecycle/menu checks and all UI, engine, canvas, placement, preview, and manual-placement suites. Shared application tests also passed after correcting a rich-text assertion, covering both platform modifier mappings, six focus contexts, edit/modal vetoes, inactive detach and cancelled close, reorder recovery, merge, and coordinated quit. The final main-checkout run passed all nine CTest suites (macapplication, macwindow, engine, canvas, UI, window placement, preview, application, and manual placement).

The rendered tab arrangement was inspected. A read-only lifecycle review identified and verified fixes for targeted detach, merge validation/order, Windows native-dialog quit handling, restored order, and independent group placement.

Real Omarchy execution was blocked by a required Tailscale reauthentication. Windows was not compiled or runtime-tested in this session. Their shortcut mappings are covered locally; device-level validation remains outstanding.

## Delivery
Changes are in the main checkout with the public README update preserved and adjusted for the new tab behavior. No commit, push, release package, or deployment was performed. Restart Mindarchy after the build to load the new interface.
