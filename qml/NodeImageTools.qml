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
    function preview(id) { imagePreview.imageSource=controller.imageSource(id); imagePreview.show(); imagePreview.requestActivate() }
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
    Window {
        id: imagePreview; objectName: "nodeImagePreview"
        property string imageSource: ""
        title: "Image Preview — Mindarchy"; width: 720; height: 540
        minimumWidth: 240; minimumHeight: 180
        transientParent: tools.hostWindow
        color: tools.controller.canvasColor
        onClosing: Qt.callLater(function() { tools.hostWindow.requestActivate(); tools.canvas.forceActiveFocus() })
        Shortcut { sequence: "Space"; context: Qt.WindowShortcut; enabled: imagePreview.visible; autoRepeat: false; onActivated: imagePreview.close() }
        Shortcut { sequence: "Escape"; context: Qt.WindowShortcut; enabled: imagePreview.visible; autoRepeat: false; onActivated: imagePreview.close() }
        Image { anchors.fill: parent; anchors.margins: 20; source: imagePreview.imageSource; fillMode: Image.PreserveAspectFit; smooth: true }
    }
}
