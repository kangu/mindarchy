import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: dialog
    objectName: "shareDialog"
    modal: true
    focus: true
    width: 400
    padding: 20

    readonly property bool shareAvailable: typeof share !== "undefined" && share !== null
    readonly property var serverModel: {
        var list = shareAvailable ? share.presets.slice() : []
        list.push("Custom…")
        return list
    }
    readonly property int customServerIndex: serverModel.length - 1
    readonly property int detectedServerIndex: {
        if (!shareAvailable) return customServerIndex
        var found = share.presets.indexOf(share.serverUrl)
        return found >= 0 ? found : customServerIndex
    }
    readonly property bool sharingActive: shareAvailable && share.mapId.length > 0

    background: Rectangle {
        color: "#172129"
        border.color: "#405461"
        border.width: 1
        radius: 8
    }
    SharedMaps {
        id: sharedMapsList
        parent: dialog.Overlay.overlay !== null ? dialog.Overlay.overlay : dialog
        model: dialog.shareAvailable ? share.sharedMaps : []
        onMapRequested: function(mapId) {
            share.joinSharedMap(mapId)
            sharedMapsList.close()
        }
    }
    contentItem: Flickable {
        id: shareFlick
        clip: true
        contentHeight: shareColumn.implicitHeight
        interactive: contentHeight > height
        implicitHeight: Math.min(shareColumn.implicitHeight, typeof Overlay !== "undefined" && Overlay.overlay ? (Overlay.overlay.height - 120) : shareColumn.implicitHeight)
        ScrollBar.vertical: ScrollBar {
            interactive: false
        }
        ColumnLayout {
            id: shareColumn
            width: shareFlick.width
            spacing: 8
        Label { text: "Share map"; font.pixelSize: 18; color: "#e0e9ee" }
        Label {
            objectName: "shareStatusLabel"
            text: dialog.shareAvailable ? share.shareStatus : "Offline"
            color: "#9bb0bb"
            Layout.fillWidth: true
        }

        Label { text: "Server"; font.pixelSize: 12; color: "#9bb0bb" }
        ComboBox {
            id: serverBox
            objectName: "shareServerBox"
            model: dialog.serverModel
            currentIndex: dialog.detectedServerIndex
            Layout.fillWidth: true
        }
        TextField {
            id: customUrl
            visible: serverBox.currentIndex === dialog.customServerIndex
            placeholderText: "https://server.example"
            Layout.fillWidth: true
            onVisibleChanged: if (visible && text.length === 0 && dialog.shareAvailable) text = share.serverUrl
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                objectName: "shareConnect"
                text: "Connect"
                enabled: dialog.shareAvailable &&
                    (customUrl.visible ? customUrl.text.trim().length > 0 : serverBox.currentText.length > 0)
                onClicked: share.chooseServer(customUrl.visible ? customUrl.text.trim() : serverBox.currentText)
            }
        }

        Loader {
            Layout.fillWidth: true
            active: dialog.shareAvailable && !share.signedIn
            sourceComponent: ColumnLayout {
                spacing: 8
                Label { text: "Sign in"; font.pixelSize: 12; color: "#9bb0bb" }
                TextField {
                    id: signInAccount
                    objectName: "shareSignInAccount"
                    placeholderText: "Account"
                    Layout.fillWidth: true
                }
                TextField {
                    id: signInPassword
                    objectName: "shareSignInPassword"
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                    Layout.fillWidth: true
                }
                Button {
                    objectName: "shareSignIn"
                    text: "Sign in"
                    Layout.alignment: Qt.AlignRight
                    enabled: signInAccount.text.trim().length > 0 && signInPassword.text.length > 0
                    onClicked: share.signIn(signInAccount.text.trim(), signInPassword.text)
                }
            }
        }
        Loader {
            Layout.fillWidth: true
            active: dialog.shareAvailable && share.signedIn
            sourceComponent: RowLayout {
                spacing: 8
                Label {
                    text: "Signed in as " + share.accountName
                    color: "#e0e9ee"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Button {
                    objectName: "shareSignOut"
                    text: "Sign out"
                    onClicked: share.signOut()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: dialog.shareAvailable && share.signedIn
            opacity: enabled ? 1 : 0.4
            Label { text: "Invite a collaborator"; font.pixelSize: 12; color: "#9bb0bb" }
            TextField {
                id: inviteAccount
                objectName: "shareAccount"
                placeholderText: "Account"
                Layout.fillWidth: true
            }
            ComboBox {
                id: inviteRole
                objectName: "shareRole"
                model: ["editor", "viewer"]
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "shareInvite"
                    text: "Invite"
                    enabled: inviteAccount.text.trim().length > 0
                    onClicked: {
                        share.inviteOnMap(inviteAccount.text.trim(), inviteRole.currentText)
                        inviteAccount.text = ""
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: dialog.shareAvailable && share.signedIn
            opacity: enabled ? 1 : 0.4
            Label { text: "Join a shared map"; font.pixelSize: 12; color: "#9bb0bb" }
            TextField {
                id: joinToken
                objectName: "shareJoinToken"
                placeholderText: "Invite token"
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "shareJoin"
                    text: "Join map"
                    enabled: joinToken.text.trim().length > 0
                    onClicked: {
                        share.acceptInvite(joinToken.text.trim())
                        joinToken.text = ""
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: dialog.shareAvailable && share.signedIn
            opacity: enabled ? 1 : 0.4
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Shared with me: " + (dialog.shareAvailable ? share.sharedMaps.length : 0)
                    color: "#9bb0bb"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Button {
                    objectName: "shareRefreshMaps"
                    text: "Refresh"
                    onClicked: share.refreshSharedMaps()
                }
            }
            Button {
                objectName: "shareOpenSharedMaps"
                text: "Open shared maps"
                onClicked: {
                    share.refreshSharedMaps()
                    sharedMapsList.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                visible: dialog.sharingActive
                text: "Shared as " + (dialog.shareAvailable ? share.mapId : "")
                color: "#e0e9ee"
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Item { visible: !dialog.sharingActive; Layout.fillWidth: true }
            Button {
                objectName: "shareShare"
                visible: !dialog.sharingActive
                text: "Share this map"
                enabled: dialog.shareAvailable && share.signedIn
                onClicked: share.shareCurrentMap()
            }
            Button {
                objectName: "shareStopSharing"
                visible: dialog.sharingActive
                text: "Stop sharing"
                onClicked: share.disconnectSharing()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                objectName: "shareClose"
                text: "Close"
                onClicked: dialog.close()
            }
        }
        }
    }
}
