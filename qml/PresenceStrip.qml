import QtQuick
import QtQuick.Controls

Row {
    id: strip
    objectName: "presenceStrip"
    property var participants: []
    spacing: 4

    function labelFor(entry) {
        const name = typeof entry === "string" ? entry : (entry && entry.name) || "Participant"
        return typeof entry === "string" ? name.slice(0, 2).toUpperCase() : (name || "?").slice(0, 1).toUpperCase()
    }

    function colorFor(entry) {
        const palette = ["#f4b8a4", "#a8dcc2", "#a9c8ec", "#e6c9f2", "#f6e2a4", "#b8e6de", "#f2c2d2", "#cdd6a4"]
        if (typeof entry !== "string") return (entry && entry.color) || "#63869a"
        let hash = 0
        for (let i = 0; i < entry.length; ++i) hash = (hash * 31 + entry.charCodeAt(i)) % 2147483647
        return palette[hash % palette.length]
    }

    function nameFor(entry) {
        return typeof entry === "string" ? entry : ((entry && entry.name) || "Participant")
    }

    Repeater {
        model: strip.participants
        delegate: Rectangle {
            width: 26; height: 26; radius: 13
            color: strip.colorFor(modelData)
            border.color: "#d7e6ed"
            Text { anchors.centerIn: parent; text: strip.labelFor(modelData); color: "white" }
            ToolTip.visible: ma.containsMouse
            ToolTip.text: strip.nameFor(modelData)
            MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true }
        }
    }
}
