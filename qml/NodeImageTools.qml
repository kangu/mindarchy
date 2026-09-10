import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: tools
    required property var canvas
    required property var controller
    required property var hostWindow
    property int targetNode: -1
    readonly property color accent: controller.canvasColor.hslLightness > 0.5 ? "#6750b8" : "#ffe08a"
    function showMenu(id, x, y) { targetNode=id; imageMenu.x=x; imageMenu.y=y; imageMenu.open() }
    function preview(id) {
        if(Qt.platform.os === "linux") {
            linuxPreview.imageSource=controller.imageSource(id)
            linuxPreview.open()
            return
        }
        imagePreview.imageSource=controller.imageSource(id)
        imagePreview.positionPanel()
        imagePreview.show()
        imagePreview.requestActivate()
    }
    TapHandler {
        enabled: imagePreview.floatingPreview && imagePreview.visible
        onPressedChanged: if(pressed) imagePreview.close()
    }
    Rectangle {
        objectName: "imageDropHighlight"
        x: tools.canvas.imageDropRect.x-4; y: tools.canvas.imageDropRect.y-4
        width: tools.canvas.imageDropRect.width+8; height: tools.canvas.imageDropRect.height+8
        visible: tools.canvas.imageDropRect.width>0
        color: "transparent"; radius: 10; border.width: 3; border.color: tools.accent
    }
    Item {
        id: selection; objectName: "imageResizeSelection"
        x: tools.canvas.imageSelectionRect.x; y: tools.canvas.imageSelectionRect.y
        width: tools.canvas.imageSelectionRect.width; height: tools.canvas.imageSelectionRect.height
        visible: width>0 && height>0
        Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: tools.accent }
        Repeater {
            model: [[0,0],[0.5,0],[1,0],[1,0.5],[1,1],[0.5,1],[0,1],[0,0.5]]
            Rectangle {
                required property var modelData
                x: modelData[0]*selection.width-width/2; y: modelData[1]*selection.height-height/2
                width: 7; height: 7; radius: 1
                color: tools.accent; border.color: tools.controller.canvasColor; border.width: 1
            }
        }
    }
    Menu {
        id: imageMenu; objectName: "nodeImageMenu"
        MenuItem { text: "Preview Image"; onTriggered: tools.preview(tools.targetNode) }
        MenuItem { text: "Replace Image…"; onTriggered: replaceDialog.open() }
        MenuItem { text: "Cut Image"; onTriggered: tools.controller.cutImage(tools.targetNode) }
        MenuItem { text: "Paste Image"; enabled: imageMenu.visible && tools.controller.clipboardHasImage(); onTriggered: tools.controller.pasteImage(tools.targetNode) }
        MenuItem { text: "Copy Image"; onTriggered: tools.controller.copyImage(tools.targetNode) }
        MenuItem { text: "Reset Image Size"; onTriggered: tools.controller.resetImageSize(tools.targetNode) }
        MenuSeparator {}
        MenuItem { text: "Remove Image"; onTriggered: tools.controller.removeImage(tools.targetNode) }
    }
    FileDialog {
        id: replaceDialog; title: "Replace node image"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.tif *.tiff)"]
        onAccepted: tools.controller.importImage(tools.targetNode,selectedFile.toString())
    }
    Popup {
        id: linuxPreview; objectName: "borderlessImagePreview"
        property string imageSource: ""
        parent: Overlay.overlay
        modal: true; focus: true; padding: 0
        popupType: Popup.Item
        width: Math.min(tools.hostWindow.width-48,1000)
        height: Math.min(tools.hostWindow.height-48,800,width/Math.max(.01,linuxImage.sourceSize.width/Math.max(1,linuxImage.sourceSize.height)))
        anchors.centerIn: parent
        background: Item {}
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: tools.canvas.forceActiveFocus()
        contentItem: Image {
            id: linuxImage
            source: linuxPreview.imageSource; fillMode: Image.PreserveAspectFit; smooth: true
            Keys.onSpacePressed: function(event) { if(!event.isAutoRepeat) linuxPreview.close(); event.accepted=true }
        }
        Shortcut { sequence: "Space"; enabled: linuxPreview.opened; autoRepeat: false; onActivated: linuxPreview.close() }
    }
    Window {
        id: imagePreview; objectName: "nodeImagePreview"
        property string imageSource: ""
        readonly property bool floatingPreview: Qt.platform.os === "osx"
        property bool wasActive: false
        flags: floatingPreview ? Qt.Tool | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint : Qt.Window
        function positionPanel() {
            if(!floatingPreview) return
            const available=tools.hostWindow.screen
            const maxWidth=Math.min(1000,available.width-100)
            const maxHeight=Math.min(800,available.height-120)
            const padding=32
            const ratio=previewImage.sourceSize.width / Math.max(1,previewImage.sourceSize.height)
            width=Math.max(240,Math.min(maxWidth,(maxHeight-padding)*ratio+padding))
            height=Math.max(180,Math.min(maxHeight,(width-padding)/Math.max(.01,ratio)+padding))
            x=Math.max(available.virtualX+24,Math.min(available.virtualX+available.width-width-24,tools.hostWindow.x+(tools.hostWindow.width-width)/2))
            y=Math.max(available.virtualY+40,Math.min(available.virtualY+available.height-height-40,tools.hostWindow.y+(tools.hostWindow.height-height)/2))
        }
        onActiveChanged: {
            if(active) wasActive=true
            else if(floatingPreview && visible && wasActive) close()
        }
        onVisibleChanged: if(!visible) wasActive=false
        title: "Image Preview — Mindarchy"; width: 720; height: 540
        minimumWidth: 240; minimumHeight: 180
        transientParent: tools.hostWindow
        color: floatingPreview ? "transparent" : tools.controller.canvasColor
        Rectangle {
            anchors.fill: parent; anchors.margins: 5
            radius: 16; color: "#38000000"
            visible: imagePreview.floatingPreview
        }
        Rectangle {
            anchors.fill: parent; anchors.margins: 8
            radius: 12; color: "#f52b2b2d"
            border.width: 1; border.color: "#66777779"
            visible: imagePreview.floatingPreview
        }
        onClosing: Qt.callLater(function() { if(!tools) return; tools.hostWindow.requestActivate(); tools.canvas.forceActiveFocus() })
        Shortcut { sequence: "Space"; context: Qt.WindowShortcut; enabled: imagePreview.visible; autoRepeat: false; onActivated: imagePreview.close() }
        Shortcut { sequence: "Escape"; context: Qt.WindowShortcut; enabled: imagePreview.visible; autoRepeat: false; onActivated: imagePreview.close() }
        Image {
            id: previewImage
            anchors.fill: parent; anchors.margins: imagePreview.floatingPreview ? 16 : 20
            source: imagePreview.imageSource; fillMode: Image.PreserveAspectFit; smooth: true
            onStatusChanged: if(status===Image.Ready) imagePreview.positionPanel()
        }
    }
}
