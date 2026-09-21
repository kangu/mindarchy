import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: dialog
    objectName: "shareDialog"
    property string mapId: ""
    property string syncStatus: "Local only"
    modal: true
    focus: true
    width: 360
    padding: 20

    background: Rectangle {
        color: "#172129"
        border.color: "#405461"
        border.width: 1
        radius: 8
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Share map"; font.pixelSize: 18; color: "#e0e9ee" }
        Label { text: dialog.syncStatus; color: "#9bb0bb" }
        Label {
            text: "Online sharing is not available in this build. Your map remains local; invitations cannot be sent yet."
            color: "#e0e9ee"; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        TextField { enabled: false; id: account; objectName: "shareAccount"; Layout.fillWidth: true; placeholderText: "Existing account" }
        ComboBox { enabled: false; id: role; objectName: "shareRole"; model: ["editor", "viewer"]; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Close"; onClicked: dialog.close() }
            Button {
                objectName: "shareInvite"
                text: "Invite"
                enabled: false
            }
        }
    }
}
