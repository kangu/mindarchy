import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mindarchy 1.0

Popup {
    id: dialog
    objectName: "shareDialog"
    modal: true
    focus: true
    width: Math.min(520, parent ? parent.width - 32 : 520)
    padding: 24
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
    Connections {
        target: dialog.parent ? dialog.parent.Window.window : null
        function onActiveFocusItemChanged() {
            var item = target.activeFocusItem
            if (!item || !dialog.visible) return
            var ancestor = item
            while (ancestor && ancestor !== body) ancestor = ancestor.parent
            if (!ancestor) return
            var y = item.mapToItem(body, 0, 0).y
            if (y < scroll.contentY) scroll.contentY = y
            else if (y + item.height > scroll.contentY + scroll.height)
                scroll.contentY = Math.min(scroll.contentHeight - scroll.height, y + item.height - scroll.height)
        }
    }
    readonly property bool shareAvailable: typeof share !== "undefined" && share !== null
    readonly property bool signedIn: shareAvailable && share.signedIn
    readonly property bool sharingActive: shareAvailable && share.mapId.length > 0
    property string pending: ""
    property string feedback: ""
    property bool feedbackError: false
    property bool settingsExpanded: false
    readonly property color ink: ShellTheme.colors["#e0e9ee"] || "#e0e9ee"
    readonly property color muted: ShellTheme.colors["#9bb0bb"] || "#9bb0bb"
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
    function begin(action) { pending = action; feedback = ""; feedbackError = false }
    onClosed: password.text = ""
    onOpened: {
        if (signedIn) share.refreshSharedMaps()
        else account.forceActiveFocus()
    }
    Connections {
        target: dialog.shareAvailable ? share : null
        function onOperationSucceeded(action) {
            if (dialog.pending !== action) return
            dialog.pending = ""
            dialog.feedbackError = false
            if (action === "invite") {
                dialog.feedback = "Invitation ready. Copy the code below and send it to your collaborator."
                inviteAccount.text = ""
            } else if (action === "share") dialog.feedback = "Your map is ready. Invite someone below."
            else if (action === "join") { dialog.feedback = "Invitation accepted."; joinToken.text = "" }
            else { dialog.feedback = "You’re signed in."; password.text = "" }
        }
        function onOperationFailed(message) {
            dialog.pending = ""
            dialog.feedbackError = true
            dialog.feedback = message
        }
    }
    background: Rectangle {
        color: ShellTheme.colors["#172129"] || "#172129"
        border.color: ShellTheme.colors["#405461"] || "#405461"
        radius: 16
    }
    contentItem: ColumnLayout {
        spacing: 16
        Label { text: "Share your map"; font.pixelSize: 24; font.bold: true; color: dialog.ink }
        Label {
            text: "Think together. Invite someone to view or edit your map."
            color: dialog.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        TabBar {
            id: tabs
            objectName: "shareTabs"
            Layout.fillWidth: true
            TabButton { text: "This map" }
            TabButton { text: "Shared with me" }
        }
        Flickable {
            id: scroll
            Layout.fillWidth: true
            implicitHeight: Math.min(body.implicitHeight, Math.max(100, (dialog.parent ? dialog.parent.height : 800) - 260))
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: body
                width: scroll.width
                spacing: 14
                RowLayout {
                    visible: dialog.shareAvailable && (dialog.signedIn || share.rememberedLogin)
                    Layout.fillWidth: true
                    Label {
                        text: dialog.signedIn ? "Signed in on this device" : "Saved login on this device"
                        color: dialog.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                    Button {
                        objectName: "shareSignOut"
                        text: "Sign out"
                        onClicked: { dialog.pending = ""; dialog.feedback = ""; share.signOut() }
                    }
                }
                ColumnLayout {
                    visible: !dialog.signedIn
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "Sign in to collaborate"; font.bold: true; color: dialog.ink }
                    Label {
                        text: "Sign in once. Your device’s secure credential vault remembers your login until you sign out."
                        color: dialog.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                    TextField {
                        id: account
                        objectName: "shareSignInAccount"
                        placeholderText: "Account name"
                        Accessible.name: "Account name"
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: password
                        objectName: "shareSignInPassword"
                        placeholderText: "Password"
                        Accessible.name: "Password"
                        echoMode: TextInput.Password
                        Layout.fillWidth: true
                        onAccepted: if (signInButton.enabled) signInButton.clicked()
                    }
                    Button {
                        id: signInButton
                        objectName: "shareSignIn"
                        text: dialog.shareAvailable && share.reconnecting ? "Connecting…" : (dialog.pending === "signin" ? "Signing in…" : "Sign in")
                        highlighted: true
                        enabled: dialog.shareAvailable && !share.reconnecting && !dialog.pending && account.text.trim().length > 0 && password.text.length > 0
                        onClicked: { dialog.begin("signin"); share.signIn(account.text.trim(), password.text) }
                    }
                }
                ColumnLayout {
                    visible: tabs.currentIndex === 0 && dialog.signedIn
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        text: dialog.sharingActive ? "This map is shared" : "Invite people to this map"
                        font.bold: true; color: dialog.ink
                    }
                    Label {
                        text: dialog.sharingActive ? (share.canInvite ? "Choose who can view or make changes." : "Only the map owner can invite more people.") : "Start sharing to upload this map to your server, then invite people by account ID."
                        wrapMode: Text.WordWrap; color: dialog.muted; Layout.fillWidth: true
                    }
                    Button {
                        objectName: "shareShare"
                        visible: !dialog.sharingActive
                        text: dialog.pending === "share" ? "Preparing map…" : "Start sharing"
                        highlighted: true
                        enabled: dialog.signedIn && !dialog.pending
                        onClicked: { dialog.begin("share"); share.shareCurrentMap() }
                    }
                    ColumnLayout {
                        visible: dialog.sharingActive && share.canInvite
                        enabled: dialog.signedIn && dialog.sharingActive && !dialog.pending
                        Layout.fillWidth: true
                        TextField {
                            id: inviteAccount
                            objectName: "shareAccount"
                            placeholderText: "Collaborator’s account ID"
                            Accessible.name: "Collaborator’s account ID"
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox {
                                id: inviteRole
                                objectName: "shareRole"
                                model: ["Can edit", "Can view"]
                                Accessible.name: "Invitation permission"
                                Layout.fillWidth: true
                            }
                            Button {
                                objectName: "shareInvite"
                                text: dialog.pending === "invite" ? "Creating…" : "Create invitation"
                                highlighted: true
                                enabled: inviteAccount.text.trim().length > 0
                                onClicked: {
                                    dialog.begin("invite")
                                    share.inviteOnMap(inviteAccount.text.trim(), inviteRole.currentIndex === 0 ? "editor" : "viewer")
                                }
                            }
                        }
                        Label {
                            text: inviteRole.currentIndex === 0 ? "Can edit: add, change and remove map content." : "Can view: read the map without making changes."
                            color: dialog.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }
                ColumnLayout {
                    visible: tabs.currentIndex === 0 && dialog.signedIn && dialog.shareAvailable && share.invitationCode.length > 0
                    Layout.fillWidth: true
                    Label { text: "Invitation code"; font.bold: true; color: dialog.ink }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: invitationCode
                            objectName: "shareInvitationCode"
                            text: dialog.shareAvailable ? share.invitationCode : ""
                            readOnly: true; selectByMouse: true
                            Accessible.name: "Invitation code to send to your collaborator"
                            Layout.fillWidth: true
                        }
                        Button {
                            text: "Copy code"
                            onClicked: { invitationCode.selectAll(); invitationCode.copy(); dialog.feedback = "Invitation code copied." }
                        }
                    }
                    Label {
                        text: "Send this code to the person you invited. It expires in 7 days and works only with their account."
                        color: dialog.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                }
                ColumnLayout {
                    visible: tabs.currentIndex === 1 && dialog.signedIn
                    Layout.fillWidth: true
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Maps available to you"; font.bold: true; color: dialog.ink; Layout.fillWidth: true }
                        Button { objectName: "shareRefreshMaps"; text: "Refresh"; onClicked: share.refreshSharedMaps() }
                    }
                    Label {
                        visible: !dialog.shareAvailable || share.sharedMaps.length === 0
                        text: "No shared maps yet. Ask someone to invite your account, then refresh."
                        wrapMode: Text.WordWrap; color: dialog.muted; Layout.fillWidth: true
                    }
                    Repeater {
                        model: dialog.shareAvailable ? share.sharedMaps : []
                        Button {
                            required property var modelData
                            text: modelData.name || "Untitled map"
                            Layout.fillWidth: true
                            onClicked: { share.joinSharedMap(modelData.id); dialog.close() }
                        }
                    }
                    Label { text: "Have an invitation code?"; color: dialog.ink }
                    TextField {
                        id: joinToken
                        objectName: "shareJoinToken"
                        placeholderText: "Paste invitation code"
                        Accessible.name: "Invitation code"
                        Layout.fillWidth: true
                    }
                    Button {
                        objectName: "shareJoin"
                        text: dialog.pending === "join" ? "Joining…" : "Accept invitation"
                        enabled: !dialog.pending && joinToken.text.trim().length > 0
                        onClicked: { dialog.begin("join"); share.acceptInvite(joinToken.text.trim()) }
                    }
                }
                Button {
                    objectName: "shareSettingsToggle"
                    text: (dialog.settingsExpanded ? "▾  " : "▸  ") + "Connection settings"
                    flat: true
                    onClicked: dialog.settingsExpanded = !dialog.settingsExpanded
                }
                ColumnLayout {
                    visible: dialog.settingsExpanded
                    Layout.fillWidth: true
                    Label { text: "Sharing server"; color: dialog.muted }
                    ComboBox {
                        id: serverBox
                        objectName: "shareServerBox"
                        model: dialog.serverModel
                        currentIndex: dialog.detectedServerIndex
                        Accessible.name: "Sharing server"
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: customUrl
                        visible: serverBox.currentIndex === dialog.customServerIndex
                        placeholderText: "https://server.example"
                        Accessible.name: "Custom sharing server address"
                        Layout.fillWidth: true
                        onVisibleChanged: if (visible && !text && dialog.shareAvailable) text = share.serverUrl
                    }
                    Button {
                        objectName: "shareConnect"
                        text: "Use this server"
                        enabled: dialog.shareAvailable && !dialog.pending && (customUrl.visible ? customUrl.text.trim().length > 0 : serverBox.currentText.length > 0)
                        onClicked: {
                            share.chooseServer(customUrl.visible ? customUrl.text.trim() : serverBox.currentText)
                            dialog.feedbackError = false
                            dialog.feedback = "Server selected. Sign in with an account on this server."
                        }
                    }
                    Label {
                        visible: dialog.signedIn
                        text: "Your account ID"
                        color: dialog.muted
                    }
                    TextField {
                        visible: dialog.signedIn
                        text: dialog.shareAvailable ? share.accountName : ""
                        readOnly: true; selectByMouse: true
                        Accessible.name: "Your account ID, select to copy"
                        Layout.fillWidth: true
                    }
                    Button {
                        objectName: "shareStopSharing"
                        visible: dialog.sharingActive
                        text: "Disconnect this map"
                        onClicked: share.disconnectSharing()
                    }
                    Label {
                        visible: dialog.sharingActive
                        text: "Disconnecting stops syncing here. Other people keep their access to the shared map."
                        color: dialog.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                }
                Label {
                    objectName: "shareFeedback"
                    visible: text.length > 0
                    text: dialog.feedback
                    color: dialog.feedbackError ? (ShellTheme.colors["#f08b83"] || "#f08b83") : dialog.ink
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                objectName: "shareStatusLabel"
                text: dialog.shareAvailable ? share.shareStatus : "Sharing unavailable"
                elide: Text.ElideRight; color: dialog.muted; Layout.fillWidth: true
            }
            Button { objectName: "shareClose"; text: "Done"; onClicked: dialog.close() }
        }
    }
}
