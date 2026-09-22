import QtQuick
import QtQuick.Controls

Popup {
    id: maps
    objectName: "sharedMaps"
    property var model: []
    signal mapRequested(string mapId)
    width: 300
    padding: 12
    background: Rectangle { color: "#172129"; border.color: "#405461"; radius: 8 }
    contentItem: Column {
        spacing: 6
        Label { text: "Shared with me"; color: "#e0e9ee"; font.pixelSize: 16 }
        Repeater {
            model: maps.model
            delegate: Button {
                width: maps.width - 24
                text: modelData.name || modelData.id || "Untitled map"
                onClicked: maps.mapRequested(modelData.id)
            }
        }
        Label { visible: maps.model.length === 0; text: "No shared maps"; color: "#9bb0bb" }
    }
}
