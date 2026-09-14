import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mindarchy 1.0

Rectangle {
    id: strip
    objectName: "documentTabStrip"
    property var model: []
    property double activeDocumentId: 0
    readonly property int count: model.length
    property int keyboardIndex: 0
    readonly property bool overflowing: scroller.contentWidth > scroller.width
    readonly property color ink: ShellTheme.colors["#e0e9ee"] || "#e0e9ee"
    readonly property color accent: ShellTheme.colors["#70d8c4"] || "#70d8c4"
    signal activateRequested(double documentId)
    signal closeRequested(double documentId)
    signal newRequested()
    signal moveRequested(double documentId, int targetIndex)
    signal detachRequested(double documentId)
    signal documentFocusRequested()
    visible: count > 1
    height: count > 1 ? 36 : 0
    implicitHeight: height
    color: ShellTheme.colors["#111920"] || "#111920"
    Accessible.role: Accessible.PageTabList
    Accessible.name: "Open documents"

    function reveal(index) {
        const tab = tabs.itemAt(index)
        if (!tab) return
        if (tab.x < scroller.contentX) scroller.contentX = tab.x
        else if (tab.x + tab.width > scroller.contentX + scroller.width)
            scroller.contentX = tab.x + tab.width - scroller.width
        scroller.contentX = Math.max(0, Math.min(scroller.contentX, scroller.contentWidth - scroller.width))
    }
    function revealActive() {
        for (let i = 0; i < count; ++i) if (model[i].id === activeDocumentId) { reveal(i); return }
    }
    function focusTab(index) {
        if (!count) return
        index = Math.max(0, Math.min(count - 1, index))
        const tab = tabs.itemAt(index)
        if (tab) { tab.forceActiveFocus(Qt.TabFocusReason); reveal(index) }
    }
    onActiveDocumentIdChanged: Qt.callLater(revealActive)
    onModelChanged: Qt.callLater(revealActive)
    onWidthChanged: Qt.callLater(revealActive)
    Keys.onEscapePressed: strip.documentFocusRequested()
    Keys.onLeftPressed: strip.focusTab(keyboardIndex - 1)
    Keys.onRightPressed: strip.focusTab(keyboardIndex + 1)
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Home) { strip.focusTab(0); event.accepted = true }
        else if (event.key === Qt.Key_End) { strip.focusTab(strip.count - 1); event.accepted = true }
    }

    Flickable {
        id: scroller; objectName: "documentTabScroller"
        anchors.left: parent.left; anchors.right: controls.left
        height: parent.height
        contentWidth: row.width; contentHeight: height
        onContentWidthChanged: Qt.callLater(strip.revealActive)
        clip: true; boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
        interactive: strip.overflowing
        Row {
            id: row
            height: scroller.height; spacing: 1
            Repeater {
                id: tabs
                model: strip.model
                delegate: Button {
                    id: tab
                    required property var modelData
                    required property int index
                    readonly property bool selected: modelData.id === strip.activeDocumentId
                    objectName: "document-tab-" + modelData.id
                    width: Math.max(120, Math.min(240, (scroller.width - Math.max(0, strip.count - 1)) / Math.max(1, strip.count)))
                    height: strip.height
                    hoverEnabled: true; focusPolicy: Qt.StrongFocus
                    leftPadding: 12; rightPadding: 30
                    text: modelData.title
                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData.title + (modelData.edited ? ", modified" : "")
                    Accessible.selected: selected
                    Accessible.onPressAction: strip.activateRequested(modelData.id)
                    onClicked: strip.activateRequested(modelData.id)
                    onActiveFocusChanged: if (activeFocus) strip.keyboardIndex = index
                    Keys.onLeftPressed: strip.focusTab(index - 1)
                    Keys.onRightPressed: strip.focusTab(index + 1)
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Home) { strip.focusTab(0); event.accepted = true }
                        else if (event.key === Qt.Key_End) { strip.focusTab(strip.count - 1); event.accepted = true }
                        else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) { tabMenu.popup(); event.accepted = true }
                    }
                    Keys.onReturnPressed: strip.activateRequested(modelData.id)
                    Keys.onEnterPressed: strip.activateRequested(modelData.id)
                    background: Rectangle {
                        color: tab.selected ? (ShellTheme.colors["#253540"] || "#253540")
                            : pointer.containsMouse ? (ShellTheme.colors["#1e2c36"] || "#1e2c36") : strip.color
                        border.width: tab.activeFocus ? 2 : tab.selected ? 1 : 0
                        border.color: tab.activeFocus ? strip.accent : (ShellTheme.colors["#34434c"] || "#34434c")
                        Rectangle { anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 2; visible: tab.selected; color: strip.accent }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; visible: !tab.selected; color: ShellTheme.colors["#34434c"] || "#34434c" }
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text { visible: tab.modelData.edited; text: "•"; color: strip.ink; Accessible.ignored: true }
                        Text { Layout.fillWidth: true; text: tab.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: strip.ink; font.pixelSize: 12 }
                    }
                    ToolTip.visible: pointer.containsMouse && !pointer.pressed
                    ToolTip.delay: 600
                    ToolTip.text: modelData.title + (modelData.edited ? " — Modified" : "")
                    MouseArea {
                        id: pointer
                        anchors.fill: parent; anchors.rightMargin: 30
                        hoverEnabled: true; acceptedButtons: Qt.LeftButton | Qt.RightButton
                        preventStealing: true
                        property real startX: 0
                        property bool dragging: false
                        onPressed: function(mouse) { startX = mouse.x; dragging = false }
                        onPositionChanged: function(mouse) {
                            if (!(pressedButtons & Qt.LeftButton)) return
                            if (Math.abs(mouse.x - startX) > 8) dragging = true
                            if (dragging) {
                                const p = mapToItem(scroller, mouse.x, mouse.y)
                                if (p.x < 24) scroller.contentX = Math.max(0, scroller.contentX - 12)
                                if (p.x > scroller.width - 24) scroller.contentX = Math.min(Math.max(0, scroller.contentWidth - scroller.width), scroller.contentX + 12)
                            }
                        }
                        onReleased: function(mouse) {
                            if (mouse.button === Qt.RightButton) { tabMenu.popup(); return }
                            if (dragging) {
                                const destination = Math.max(0, Math.min(strip.count - 1, Math.floor((tab.x + mouse.x) / (tab.width + row.spacing))))
                                if (destination !== tab.index) strip.moveRequested(tab.modelData.id, destination)
                            } else strip.activateRequested(tab.modelData.id)
                            dragging = false
                        }
                        onCanceled: dragging = false
                    }
                    ToolButton {
                        id: closeButton
                        objectName: "close-document-tab-" + tab.modelData.id
                        anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter
                        width: 26; height: 26; text: "×"
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: "Close " + tab.modelData.title
                        onClicked: strip.closeRequested(tab.modelData.id)
                        Keys.onEscapePressed: strip.documentFocusRequested()
                        ToolTip.visible: hovered; ToolTip.delay: 500; ToolTip.text: "Close " + tab.modelData.title
                        background: Rectangle { radius: 4; color: closeButton.hovered ? (ShellTheme.colors["#34434c"] || "#34434c") : "transparent"; border.width: closeButton.activeFocus ? 2 : 0; border.color: strip.accent }
                    }
                    Menu {
                        id: tabMenu
                        MenuItem { text: "Close Document"; onTriggered: strip.closeRequested(tab.modelData.id) }
                        MenuItem { text: "Move to New Window"; enabled: strip.count > 1; onTriggered: strip.detachRequested(tab.modelData.id) }
                    }
                }
            }
        }
    }
    Row {
        id: controls
        anchors.right: parent.right; height: parent.height
        ToolButton {
            id: listButton; objectName: "documentTabListButton"
            width: visible ? 32 : 0; height: parent.height; visible: strip.overflowing
            text: "▾"; Accessible.name: "List open documents"
            ToolTip.visible: hovered; ToolTip.text: "List open documents"
            onClicked: documentList.popup()
            Menu {
                id: documentList
                Instantiator {
                    model: strip.model
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.title + (modelData.edited ? " — Modified" : "")
                        checkable: true; checked: modelData.id === strip.activeDocumentId
                        onTriggered: strip.activateRequested(modelData.id)
                    }
                    onObjectAdded: function(index, object) { documentList.insertItem(index, object) }
                    onObjectRemoved: function(index, object) { documentList.removeItem(object) }
                }
            }
        }
        ToolButton {
            objectName: "newTabButton"
            width: 36; height: parent.height; text: "+"
            Accessible.name: "New document tab"
            ToolTip.visible: hovered; ToolTip.text: "New document tab"
            onClicked: strip.newRequested()
        }
    }
}
