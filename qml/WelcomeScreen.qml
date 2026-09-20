import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Mindarchy 1.0

FocusScope {
    id: home
    objectName: "welcomeScreen"
    required property var host
    property var maps: []
    readonly property color ink: host.ink
    readonly property color muted: host.muted
    readonly property color surface: ShellTheme.colors["#172129"] || "#172129"
    readonly property color line: ShellTheme.colors["#2a3b46"] || "#2a3b46"
    property string errorText: ""
    function refresh() { maps = host.controller ? host.controller.recentMaps() : [] }
    property var pickerFocus: null
    function openDocument() { pickerFocus = host.activeFocusItem; picker.open() }
    function restorePickerFocus() {
        host.requestActivate()
        pickerFocusTimer.restart()
    }
    function control(slot) { return slot === 0 ? newMap : slot === 1 ? openMap : cards.itemAt(slot - 2) }
    function focusControl(item) {
        if (!item || !item.enabled) return
        item.forceActiveFocus(Qt.TabFocusReason)
        scrollFocusTimer.restart()
    }
    function ensureVisible(item) {
        if (!host.welcomeVisible || !item || !item.activeFocus) return
        var flick = scroll.contentItem
        var y = item.mapToItem(flick.contentItem, 0, 0).y
        var target = flick.contentY
        if (y < target + 8) target = y - 8
        else if (y + item.height > target + flick.height - 8) target = y + item.height - flick.height + 8
        flick.contentY = Math.max(0, Math.min(target, flick.contentHeight - flick.height))
    }
    function focusCard(index, step) {
        while (index >= 0 && index < 6) {
            var item = cards.itemAt(index)
            if (item && item.enabled) { focusControl(item); return true }
            index += step
        }
        return false
    }
    function navigate(event, slot) {
        if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.AltModifier)) return
        var key = event.key
        if (key === Qt.Key_Tab || key === Qt.Key_Backtab) {
            event.accepted = true
            var direction = key === Qt.Key_Backtab || (event.modifiers & Qt.ShiftModifier) ? -1 : 1
            for (var i = 1; i <= 8; ++i) {
                var next = control((slot + i * direction + 8) % 8)
                if (next && next.enabled) { focusControl(next); return }
            }
        } else if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) {
            event.accepted = true
            if (!event.isAutoRepeat) control(slot).clicked()
        } else if (key === Qt.Key_Escape) {
            event.accepted = true; focusControl(newMap)
        } else if (key === Qt.Key_Home || key === Qt.Key_End) {
            event.accepted = true
            if (!focusCard(key === Qt.Key_Home ? 0 : 5, key === Qt.Key_Home ? 1 : -1)) focusControl(newMap)
        } else if ([Qt.Key_Left,Qt.Key_Right,Qt.Key_Up,Qt.Key_Down].indexOf(key) >= 0) {
            event.accepted = true
            if (slot < 2) {
                if (key === Qt.Key_Down) focusCard(0,1)
                else if (key === Qt.Key_Left) focusControl(openMap)
                else if (key === Qt.Key_Right) focusControl(newMap)
                return
            }
            var index = slot - 2
            if (key === Qt.Key_Up && index < grid.columns) { focusControl(newMap); return }
            var step = key === Qt.Key_Up ? -grid.columns : key === Qt.Key_Down ? grid.columns : key === Qt.Key_Left ? -1 : 1
            // Horizontal movement stays in its row; vertical movement follows columns.
            var nextIndex = index + step
            while (nextIndex >= 0 && nextIndex < 6) {
                if (Math.abs(step) === 1 && (key === Qt.Key_Left || key === Qt.Key_Right) && Math.floor(nextIndex/grid.columns) !== Math.floor(index/grid.columns)) break
                var target = cards.itemAt(nextIndex)
                if (target && target.enabled) { focusControl(target); break }
                nextIndex += step
            }
        }
    }
    function openMap(path) {
        if (host.controller.open(path)) { errorText = ""; host.welcomeVisible = false; host.focusDocument() }
        else errorText = host.controller.error
    }
    Connections { target: home.host; function onActiveChanged() { if (home.host.active) home.refresh() } }
    Timer { id: scrollFocusTimer; interval: 0; onTriggered: home.ensureVisible(home.host.activeFocusItem) }
    Timer { id: pickerFocusTimer; interval: 0; onTriggered: if (home.visible) home.focusControl(home.pickerFocus || openMap) }
    Timer { id: initialFocusTimer; interval: 0; onTriggered: if (!home.maps.length || !home.focusCard(0,1)) home.focusControl(newMap) }
    Component.onCompleted: { refresh(); initialFocusTimer.start() }
    FileDialog {
        id: picker; objectName: "welcomeFilePicker"; title: "Open a mindmap"
        nameFilters: ["Mindarchy maps (*.omm *.json)"]
        onAccepted: { home.openMap(host.activeWorkspace.localPath(selectedFile)); if (host.welcomeVisible) home.restorePickerFocus() }
        onRejected: home.restorePickerFocus()
    }
    Rectangle { anchors.fill: parent; color: host.color }
    // The native caption controls retain the same reserved toolbar space.
    RowLayout {
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: host.integratedMacToolbar ? 108 : 32
        anchors.rightMargin: 28 + host.windowsCaptionWidth
        height: 64
        DragHandler { target: null; acceptedButtons: Qt.LeftButton; onActiveChanged: if (active) home.host.startSystemMove() }
        Label { text: "mindarchy"; color: home.ink; font.pixelSize: 17; font.weight: Font.DemiBold; font.letterSpacing: -.4 }
        Item { Layout.fillWidth: true }
        Label { text: "A little space to think."; color: home.muted; font.pixelSize: 12; visible: home.width > 760 }
    }
    ScrollView {
        id: scroll
        anchors.fill: parent; anchors.topMargin: 90
        clip: true; ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        contentWidth: availableWidth
        Connections {
            target: scroll.contentItem
            function onContentHeightChanged() { scrollFocusTimer.restart() }
            function onHeightChanged() { scrollFocusTimer.restart() }
        }
        ColumnLayout {
            width: Math.min(1180, scroll.availableWidth - (home.width < 700 ? 40 : 88))
            x: (scroll.availableWidth-width)/2
            spacing: 24
            RowLayout {
                Layout.fillWidth: true; spacing: 20
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 9
                    Label { text: "Welcome back."; color: home.ink; font.pixelSize: home.width < 700 ? 28 : 36; font.weight: Font.DemiBold; font.letterSpacing: -1 }
                    Label { text: home.maps.length ? "Pick up a thought. Or start something new." : "Your next idea starts here."; color: home.muted; font.pixelSize: 14; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
                Button {
                    id: openMap; objectName: "welcomeOpenMap"
                    focusPolicy: Qt.StrongFocus
                    Keys.onPressed: function(event) { home.navigate(event,1) }
                    onActiveFocusChanged: if (activeFocus) home.ensureVisible(openMap)
                    text: "Open map"; Accessible.name: text
                    onClicked: home.openDocument()
                    padding: 14
                    background: Rectangle { radius: 8; color: openMap.hovered ? home.surface : "transparent"; border.color: openMap.activeFocus ? home.ink : home.line }
                    contentItem: Text { text: openMap.text; color: home.ink; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter }
                }
                Button {
                    id: newMap; objectName: "welcomeNewMap"
                    focusPolicy: Qt.StrongFocus
                    Keys.onPressed: function(event) { home.navigate(event,0) }
                    onActiveFocusChanged: if (activeFocus) home.ensureVisible(newMap)
                    text: "+  New map"; Accessible.name: "Create a new map"
                    onClicked: host.startWelcomeMap()
                    padding: 14
                    background: Rectangle { radius: 8; color: newMap.hovered ? "#b2e4ce" : "#98d5bd"; border.width: newMap.activeFocus ? 2 : 0; border.color: home.ink }
                    contentItem: Text { text: newMap.text; color: "#142a23"; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
                }
            }
            Label { text: home.errorText; visible: text.length > 0; color: "#ed9e91"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 12
                Label { text: "RECENT MAPS"; color: home.muted; font.pixelSize: 10; font.letterSpacing: 2; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                Label { text: home.maps.length ? "Most recent first" : "Your maps will appear here"; color: home.muted; font.pixelSize: 11 }
            }
            GridLayout {
                id: grid; objectName: "welcomeGrid"
                Layout.fillWidth: true
                columns: home.width >= 960 ? 3 : home.width >= 620 ? 2 : 1
                rowSpacing: 20; columnSpacing: 20
                Repeater {
                    id: cards
                    model: 6
                    delegate: Button {
                        id: card
                        required property int index
                        readonly property var map: index < home.maps.length ? home.maps[index] : null
                        objectName: "welcomeCard" + index
                        Layout.fillWidth: true; Layout.preferredWidth: 1
                        Layout.preferredHeight: Math.max(190, Math.min(280, (home.height-285)/2))
                        padding: 0; hoverEnabled: true; focusPolicy: Qt.StrongFocus
                        Keys.onPressed: function(event) { home.navigate(event,index+2) }
                        onActiveFocusChanged: if (activeFocus) home.ensureVisible(card)
                        Accessible.description: "Recent map slot " + (index+1) + " of 6. Use arrow keys to navigate and Enter to open."
                        enabled: !map || map.available
                        Accessible.name: map ? "Open " + map.name + (map.available ? "" : ", file unavailable") : "Create a new map"
                        onClicked: { if (map) home.openMap(map.path); else host.startWelcomeMap() }
                        background: Rectangle {
                            color: home.surface; radius: 12
                            border.width: card.activeFocus ? 2 : 1
                            border.color: card.activeFocus ? home.ink : card.hovered ? home.muted : home.line
                            Behavior on border.color { ColorAnimation { duration: 120 } }
                        }
                        contentItem: Item {
                            Rectangle {
                                anchors.fill: parent; anchors.margins: 12; anchors.bottomMargin: 64
                                radius: 6; color: home.host.color; clip: true
                                Image {
                                    id: snapshot; objectName: "welcomeThumbnail" + card.index
                                    anchors.fill: parent
                                    source: card.map && card.map.available ? card.map.thumbnail : ""
                                    sourceSize: Qt.size(900,550); fillMode: Image.PreserveAspectFit
                                    asynchronous: true; cache: true
                                    opacity: status === Image.Ready ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: 180 } }
                                }
                                Label {
                                    anchors.centerIn: parent; color: home.muted; font.pixelSize: card.map ? 12 : 26
                                    visible: snapshot.status !== Image.Ready
                                    text: !card.map ? "+" : !card.map.available ? "File unavailable" : snapshot.status === Image.Error ? "Preview unavailable" : "Rendering preview…"
                                }
                            }
                            Column {
                                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                                anchors.margins: 17; spacing: 5
                                Label { width: parent.width; text: card.map ? card.map.name : "Room for a new idea"; textFormat: Text.PlainText; elide: Text.ElideRight; color: card.map ? home.ink : home.muted; font.pixelSize: 14; font.weight: Font.Medium }
                                Label { width: parent.width; text: card.map ? (card.map.available ? "Modified " + card.map.modified : "Use Open map to locate this file") : "Create a mindmap"; color: home.muted; font.pixelSize: 11; elide: Text.ElideRight }
                            }
                        }
                        ToolTip.visible: hovered && map !== null
                        ToolTip.delay: 700; ToolTip.text: map ? map.path : ""
                    }
                }
            }
            Label { Layout.fillWidth: true; text: "Arrow keys to explore · Enter to open · Tab for actions"; color: home.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
            Item { Layout.preferredHeight: 24 }
        }
    }
}
