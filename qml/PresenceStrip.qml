import QtQuick
import QtQuick.Controls

Row {
    id: strip
    objectName: "presenceStrip"
    property var participants: []
    spacing: 4
    Repeater {
        model: strip.participants
        delegate: Rectangle {
            width: 26; height: 26; radius: 13
            color: modelData.color || "#63869a"
            border.color: "#d7e6ed"
            Text { anchors.centerIn: parent; text: (modelData.name || "?").slice(0, 1).toUpperCase(); color: "white" }
            ToolTip.visible: ma.containsMouse
            ToolTip.text: modelData.name || "Participant"
            MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true }
        }
    }
}
