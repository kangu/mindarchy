import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: shortcuts
    objectName: "windowsKeyboardShortcuts"
    title: qsTr("Keyboard Shortcuts — Mindarchy")
    width: 720; height: 660
    minimumWidth: 600; minimumHeight: 420
    visible: false
    color: "#172129"
    readonly property var rows: [
        ["Document", "New document", "Ctrl+N"],
        ["Document", "Open document", "Ctrl+O"],
        ["Document", "Save", "Ctrl+S"],
        ["Document", "Close window", "Ctrl+W"],
        ["Document", "Undo / redo", "Ctrl+Z / Ctrl+Y"],
        ["Canvas", "Navigate nodes", "Arrow keys"],
        ["Canvas", "Extend selection", "Shift+Arrow keys"],
        ["Canvas", "Pan view", "Ctrl+Arrow keys / Space+drag"],
        ["Canvas", "Add child / sibling", "Tab / Enter"],
        ["Canvas", "Edit node", "Ctrl+Enter / F2"],
        ["Canvas", "Delete selection", "Delete / Backspace"],
        ["Canvas", "Fold branch / toggle task", "F / T"],
        ["Canvas", "Zoom in / zoom out / fit", "+ or = / − / 0"],
        ["Canvas", "Clear selection or cancel drag", "Esc"],
        ["Search", "Find / next result", "Ctrl+F / Enter"],
        ["Search", "Close search", "Esc"],
        ["Node editing", "Finish editing", "Enter / Esc"],
        ["Node editing", "Finish and add child", "Tab"],
        ["Node editing", "Insert line break", "Shift+Enter"],
        ["Node editing", "Bold / italic / underline", "Ctrl+B / Ctrl+I / Ctrl+U"],
        ["Text fields", "Select all", "Ctrl+A"],
        ["Text fields", "Cut / copy / paste", "Ctrl+X / Ctrl+C / Ctrl+V"],
        ["Date entry", "Save / cancel", "Ctrl+Enter / Esc"],
        ["Window", "Show or hide menu bar", "Alt"],
        ["Window", "Dismiss menu", "Esc"]
    ]
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        Label { text: qsTr("Keyboard shortcuts"); color: "#e0e9ee"; font.pixelSize: 24; font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("CONTEXT"); color: "#81939f"; Layout.preferredWidth: 108 }
            Label { text: qsTr("ACTION"); color: "#81939f"; Layout.fillWidth: true }
            Label { text: qsTr("SHORTCUT"); color: "#81939f"; Layout.preferredWidth: 236 }
        }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            model: shortcuts.rows
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width; height: 44
                color: index % 2 ? "#1e2c36" : "#172129"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 10
                    Label { text: modelData[0]; color: "#81939f"; Layout.preferredWidth: 100; font.pixelSize: 12 }
                    Label { text: modelData[1]; color: "#e0e9ee"; Layout.fillWidth: true; font.pixelSize: 12 }
                    Label { text: modelData[2]; color: "#70d8c4"; Layout.preferredWidth: 228; font.pixelSize: 12 }
                }
            }
        }
        Button { text: qsTr("Close"); Layout.alignment: Qt.AlignRight; onClicked: shortcuts.close() }
    }
    Shortcut { sequence: "Escape"; onActivated: shortcuts.close() }
}
