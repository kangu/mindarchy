import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: dialog
    objectName: "shareDialog"
    property string mapId: ""
    property string syncStatus: "Local only"
    signal inviteRequested(string account, string role)
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
        TextField { id: account; objectName: "shareAccount"; Layout.fillWidth: true; placeholderText: "Existing account" }
        ComboBox { id: role; objectName: "shareRole"; model: ["editor", "viewer"]; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Cancel"; onClicked: dialog.close() }
            Button {
                text: "Invite"
                enabled: account.text.trim().length > 0
                onClicked: { dialog.inviteRequested(account.text.trim(), role.currentText); dialog.close() }
            }
        }
    }
}
