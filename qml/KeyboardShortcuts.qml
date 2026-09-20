import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mindarchy 1.0

Window {
    id: shortcuts
    objectName: "windowsKeyboardShortcuts"
    title: qsTr("Keyboard Shortcuts — Mindarchy")
    width: 720; height: 660
    minimumWidth: 600; minimumHeight: 420
    visible: false
    onVisibleChanged: { if(!visible && transientParent) Qt.callLater(function() { shortcuts.transientParent.requestActivate() }) }
    color: (ShellTheme.colors["#172129"] || "#172129")
    property var controller: typeof engine !== "undefined" ? engine : null
    readonly property var rows: (controller ? controller.tabShortcutHelp.map(function(entry) {
        return ["Document tabs", entry.label, entry.shortcut]
    }) : []).concat([
        ["Welcome", "Move between map cards", "Arrow keys"],
        ["Welcome", "Next / previous action or card", "Tab / Shift+Tab"],
        ["Welcome", "Open focused map or activate action", "Enter / Space"],
        ["Welcome", "First / last card", "Home / End"],
        ["Welcome", "Focus New Map", "Esc"],
        ["Welcome", "New map / open file", "Ctrl+N / Ctrl+O"],
        ["Document", "Open document", "Ctrl+O"],
        ["Document", "Save", "Ctrl+S"],
        ["Document", "Undo / redo", "Ctrl+Z / Ctrl+Y"],
        ["Canvas", "Copy / paste branches as children", "Ctrl+C / Ctrl+V"],
        ["Images", "Copy / cut selected image; paste onto selected node", "Ctrl+C / Ctrl+X / Ctrl+V"],
        ["Canvas", "Connect two selected nodes", "Ctrl+L"],
        ["Canvas", "Toggle branch Focus mode", "Ctrl+Shift+F"],
        ["Canvas", "Exit Focus and restore viewport", "Esc"],
        ["Canvas", "Navigate nodes", "Arrow keys"],
        ["Canvas", "Extend selection", "Shift+Arrow keys"],
        ["Canvas", "Pan view", "Ctrl+Arrow keys / Space+drag"],
        ["Canvas", "Add child / sibling", "Tab / Enter"],
        ["Canvas", "Edit node", "Ctrl+Enter / F2"],
        ["Canvas", "Delete selected branch", "Delete / Backspace"],
        ["Images", "Remove selected image, keeping its node", "Delete / Backspace"],
        ["Canvas", "Replace selected title", "Type text"],
        ["Canvas", "Fold branch / toggle task", "Alt+F / Alt+T"],
        ["Canvas", "Zoom in / zoom out", "+ or = / −"],
        ["Canvas (no selection)", "Fit map", "0"],
        ["Canvas", "Clear selection or cancel drag", "Esc"],
        ["Search", "Find / next result", "Ctrl+F / Enter"],
        ["Search", "Close search", "Esc"],
        ["Selected image", "Open image preview", "Space"],
        ["Image preview", "Close preview", "Space / Esc"],
        ["Image resizing", "Cancel resize", "Esc"],
        ["Node editing", "Finish editing", "Enter / Esc"],
        ["Node editing", "Finish and add child", "Tab"],
        ["Node editing", "Insert line break", "Shift+Enter"],
        ["Node editing", "Bold / italic / underline", "Ctrl+B / Ctrl+I / Ctrl+U"],
        ["Text fields", "Select all", "Ctrl+A"],
        ["Text fields", "Cut / copy / paste", "Ctrl+X / Ctrl+C / Ctrl+V"],
        ["Date entry", "Save / cancel", "Ctrl+Enter / Esc"],
        ["Window", Qt.platform.os === "linux" ? "Open application menu" : "Show or hide menu bar", Qt.platform.os === "linux" ? "Header menu button" : "Alt"],
        ["Window", "Dismiss menu", "Esc"],
        ["Tab strip focus", "Focus first / last / adjacent tab", "Home / End / Left / Right"],
        ["Tab strip focus", "Activate tab / return to document", "Enter or Space / Esc"]
    ].map(function(row) {
        return Qt.platform.os === "osx" ? [row[0], row[1], row[2].replace(/Ctrl\+/g, "⌘ ")] : row
    }))
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        Label { text: qsTr("Keyboard shortcuts"); color: (ShellTheme.colors["#e0e9ee"] || "#e0e9ee"); font.pixelSize: 24; font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("CONTEXT"); color: (ShellTheme.colors["#81939f"] || "#81939f"); Layout.preferredWidth: 108 }
            Label { text: qsTr("ACTION"); color: (ShellTheme.colors["#81939f"] || "#81939f"); Layout.fillWidth: true }
            Label { text: qsTr("SHORTCUT"); color: (ShellTheme.colors["#81939f"] || "#81939f"); Layout.preferredWidth: 236 }
        }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            model: shortcuts.rows
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width; height: 44
                color: index % 2 ? (ShellTheme.colors["#1e2c36"] || "#1e2c36") : (ShellTheme.colors["#172129"] || "#172129")
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 10
                    Label { text: modelData[0]; color: (ShellTheme.colors["#81939f"] || "#81939f"); Layout.preferredWidth: 100; font.pixelSize: 12 }
                    Label { text: modelData[1]; color: (ShellTheme.colors["#e0e9ee"] || "#e0e9ee"); Layout.fillWidth: true; font.pixelSize: 12 }
                    Label { text: modelData[2]; color: (ShellTheme.colors["#70d8c4"] || "#70d8c4"); Layout.preferredWidth: 228; font.pixelSize: 12 }
                }
            }
        }
        Button { text: qsTr("Close"); Layout.alignment: Qt.AlignRight; onClicked: shortcuts.close() }
    }
    Shortcut { sequence: "Escape"; onActivated: shortcuts.close() }
}
