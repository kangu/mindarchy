import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Mindarchy 1.0

ApplicationWindow {
    id: window
    width: 1380; height: 900
    minimumWidth: 600; minimumHeight: 640
    visible: typeof deferWindowShow === "undefined" || !deferWindowShow
    readonly property bool integratedMacToolbar: Qt.platform.os === "osx"
    readonly property bool integratedWindowsToolbar: Qt.platform.os === "windows"
    readonly property bool integratedToolbar: integratedMacToolbar || integratedWindowsToolbar
    // Keep the native resize frame without a separate caption or caption overlay.
    flags: integratedWindowsToolbar
        ? Qt.Window | Qt.CustomizeWindowHint | Qt.WindowSystemMenuHint
        : integratedMacToolbar
          ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
          : Qt.Window
    readonly property real windowsCaptionWidth: integratedWindowsToolbar && visibility !== Window.FullScreen ? 138 : 0
    // The toolbar reserves horizontal space for the native window controls.
    // Avoid ApplicationWindow's automatic inset below the macOS title bar.
    Binding { target: window; property: "topPadding"; value: 0; when: window.integratedToolbar }
    title: controller ? controller.documentName : "Mindarchy"
    property bool windowsMenuVisible: false
    property var windowsMenuPreviousFocus: null
    readonly property real windowsMenuHeight: windowsMenus.height
    readonly property bool windowsMenuPopupOpen: windowsMenus.item ? windowsMenus.item.popupOpen : false
    function showWindowsMenu() {
        if (Qt.platform.os !== "windows") return
        windowsMenuPreviousFocus = activeFocusItem
        windowsMenuVisible = true
        if (windowsMenus.item) windowsMenus.item.itemAt(0).forceActiveFocus()
    }
    function hideWindowsMenu(restoreFocus) {
        windowsMenuVisible = false
        if (windowsMenus.item) windowsMenus.item.closeMenus()
        if (restoreFocus && windowsMenuPreviousFocus) windowsMenuPreviousFocus.forceActiveFocus()
        windowsMenuPreviousFocus = null
    }
    menuBar: Loader {
        id: windowsMenus
        active: Qt.platform.os === "windows"
        visible: active && window.windowsMenuVisible
        height: visible && item ? item.implicitHeight : 0
        sourceComponent: MenuBar {
            id: windowsMenuContent
            readonly property bool popupOpen: window.activeWorkspace ? window.activeWorkspace.desktopMenuSet.popupOpen : false
            function closeMenus() { if (window.activeWorkspace) window.activeWorkspace.desktopMenuSet.closeMenus() }
            property var installedMenus: null
            function syncMenus() {
                if (installedMenus) { removeMenu(installedMenus.fileMenu); removeMenu(installedMenus.windowMenu); removeMenu(installedMenus.helpMenu) }
                installedMenus = window.activeWorkspace ? window.activeWorkspace.desktopMenuSet : null
                if (installedMenus) { addMenu(installedMenus.fileMenu); addMenu(installedMenus.windowMenu); addMenu(installedMenus.helpMenu) }
            }
            Connections { target: window; function onActiveWorkspaceChanged() { windowsMenuContent.syncMenus() } }
            Component.onCompleted: syncMenus()

        }
    }
    color: ShellTheme.colors["#111920"] || "#111920"
    property var activeWorkspace: defaultWorkspace.item
    property var controller: activeWorkspace ? activeWorkspace.controller : (typeof engine !== "undefined" ? engine : null)
    onControllerChanged: if (activeWorkspace && activeWorkspace.controller !== controller) activeWorkspace.controller = controller
    property var documentTabs: []
    property double documentTabId: 0
    property bool canMergeWindows: false
    property bool outlineVisible: false
    property bool inspectorVisible: true
    property bool allowClose: false
    readonly property bool documentEdited: activeWorkspace ? activeWorkspace.documentEdited : false
    readonly property bool quitPending: activeWorkspace ? activeWorkspace.quitPending : false
    readonly property bool modalInteraction: activeWorkspace ? activeWorkspace.modalInteraction : false
    readonly property color ink: ShellTheme.colors["#e0e9ee"] || "#e0e9ee"
    readonly property color muted: ShellTheme.colors["#81939f"] || "#81939f"
    signal tabMoveRequested(double documentId, int targetIndex)
    onClosing: function(event) {
        if (allowClose || !activeWorkspace) return
        event.accepted = false
        activeWorkspace.requestClose(false, false)
    }
    Loader {
        id: defaultWorkspace
        anchors.fill: parent
        active: typeof sharedWindowManaged === "undefined" || !sharedWindowManaged
        sourceComponent: DocumentWorkspace {
            hostWindow: window
            onCloseApproved: { window.allowClose = true; window.close() }
        }
    }
    palette.window: (ShellTheme.colors["#172129"] || "#172129")
    palette.windowText: ink
    palette.base: (ShellTheme.colors["#111b23"] || "#111b23")
    palette.alternateBase: (ShellTheme.colors["#1e2c36"] || "#1e2c36")
    palette.text: ink
    palette.button: (ShellTheme.colors["#253540"] || "#253540")
    palette.buttonText: ink
    palette.highlight: (ShellTheme.colors["#317d73"] || "#317d73")
    palette.dark: (ShellTheme.colors["#253540"] || "#253540")
    palette.light: (ShellTheme.colors["#2a3d48"] || "#2a3d48")
    palette.midlight: (ShellTheme.colors["#32434d"] || "#32434d")
    palette.brightText: ink
    palette.toolTipBase: (ShellTheme.colors["#172129"] || "#172129")
    palette.toolTipText: ink
    palette.placeholderText: muted
    palette.mid: (ShellTheme.colors["#34434c"] || "#34434c")
    palette.highlightedText: (ShellTheme.colors["#ffffff"] || "#ffffff")
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "Sans Serif"
    font.pixelSize: 13

    property bool searchOpen: activeWorkspace ? activeWorkspace.searchOpen : false
    onSearchOpenChanged: if (activeWorkspace && activeWorkspace.searchOpen !== searchOpen) activeWorkspace.searchOpen = searchOpen
    property bool useThemeLayouts: activeWorkspace ? activeWorkspace.useThemeLayouts : false
    onUseThemeLayoutsChanged: if (activeWorkspace && activeWorkspace.useThemeLayouts !== useThemeLayouts) activeWorkspace.useThemeLayouts = useThemeLayouts
    readonly property int searchIndex: activeWorkspace ? activeWorkspace.searchIndex : -1
    function recoveryDraft() { if (activeWorkspace) return activeWorkspace.recoveryDraft() }
    function restoreRecoveryDraft(state) { if (activeWorkspace) return activeWorkspace.restoreRecoveryDraft(state) }
    function prepareRecoveryQuit() { if (activeWorkspace) return activeWorkspace.prepareRecoveryQuit() }
    function abortSessionQuit() { if (activeWorkspace) return activeWorkspace.abortSessionQuit() }
    function completeSessionQuit() { if (activeWorkspace) return activeWorkspace.completeSessionQuit() }
    function saveDocument(closing) { if (activeWorkspace) return activeWorkspace.saveDocument(closing) }
    function openDocumentMenu() { if (activeWorkspace) return activeWorkspace.openDocumentMenu() }
    function requestClose(forget, quitting) { if (activeWorkspace) return activeWorkspace.requestClose(forget, quitting) }
    function approveClose() { if (activeWorkspace) return activeWorkspace.approveClose() }
    function cancelClose() { if (activeWorkspace) return activeWorkspace.cancelClose() }
    function saveBeforeClosing() { if (activeWorkspace) return activeWorkspace.saveBeforeClosing() }
    function finishSaveDialog(path) { if (activeWorkspace) return activeWorkspace.finishSaveDialog(path) }
    function commitForTabSwitch() { if (activeWorkspace) return activeWorkspace.commitForTabSwitch() }
    function commitEditor(next) { if (activeWorkspace) return activeWorkspace.commitEditor(next) }
    function applyTheme(id) { if (activeWorkspace) return activeWorkspace.applyTheme(id) }
    function focusDocument() { if (activeWorkspace) return activeWorkspace.focusDocument() }
}
