import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Mindarchy 1.0

ColumnLayout {
    id: panel
    required property var controller
    required property var commitEditor
    readonly property bool editingResource: resourceDialog.opened
    spacing: 8
    enabled: controller.selection.length === 1
    Label { text: "LINKS & RESOURCES"; color: ShellTheme.colors["#94a9b7"] || "#94a9b7"; font.pixelSize: 10; font.letterSpacing: 1.5 }
    Label {
        visible: controller.selectedResources.length === 0
        text: "Keep references with this idea."; wrapMode: Text.WordWrap; Layout.fillWidth: true
        color: ShellTheme.colors["#94a9b7"] || "#94a9b7"
    }
    Repeater {
        model: controller.selectedResources
        delegate: Rectangle {
            required property var modelData
            required property int index
            Layout.fillWidth: true; implicitHeight: entry.implicitHeight + 20
            radius: 8; color: ShellTheme.colors["#203640"] || "#203640"
            ColumnLayout {
                id: entry; anchors.fill: parent; anchors.margins: 10; spacing: 4
                Label {
                    Layout.fillWidth: true; elide: Text.ElideRight; font.bold: true; textFormat: Text.PlainText
                    text: modelData.name || (modelData.kind === "file" ? modelData.target.split(/[\\/]/).pop() : modelData.target)
                }
                Label {
                    Layout.fillWidth: true; elide: Text.ElideMiddle; textFormat: Text.PlainText; text: modelData.target
                    font.pixelSize: 11; color: ShellTheme.colors["#94a9b7"] || "#94a9b7"
                    HoverHandler { id: targetHover }
                    ToolTip.visible: targetHover.hovered; ToolTip.delay: 0; ToolTip.text: modelData.target
                }
                RowLayout {
                    Button { objectName: "openResource" + index; Layout.fillWidth: true; Layout.preferredWidth: 0; text: "Open"; onClicked: controller.openResource(controller.selectedId,index) }
                    Button { objectName: "editResource" + index; Layout.fillWidth: true; Layout.preferredWidth: 0; text: "Edit"; onClicked: panel.edit(index,modelData.kind,modelData.name,modelData.target) }
                    Button { objectName: "removeResource" + index; Layout.fillWidth: true; Layout.preferredWidth: 0; text: "Remove"; onClicked: { if(panel.commitEditor("")) controller.removeResource(controller.selectedId,index) } }
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { objectName: "addWebResource"; text: "Add link"; Layout.fillWidth: true; onClicked: panel.edit(-1,"url","","") }
        Button { objectName: "addFileResource"; text: "Add file"; Layout.fillWidth: true; onClicked: panel.edit(-1,"file","","") }
    }
    function edit(index,kind,name,target) {
        if(!commitEditor("")) return
        resourceDialog.nodeId=controller.selectedId
        resourceDialog.resourceIndex=index
        resourceDialog.kind=kind
        resourceName.text=name; resourceTarget.text=target
        resourceDialog.open()
        resourceTarget.forceActiveFocus()
    }
    Dialog {
        id: resourceDialog; objectName: "resourceDialog"; parent: Overlay.overlay
        property int nodeId: -1
        property int resourceIndex: -1
        property string kind: "url"
        title: (resourceIndex < 0 ? "Add " : "Edit ") + (kind === "file" ? "file reference" : "web link")
        modal: true; width: Math.min(480,parent.width-40)
        x: (parent.width-width)/2; y: (parent.height-height)/2
        contentItem: ColumnLayout {
            spacing: 12
            TextField { id: resourceName; objectName: "resourceName"; Layout.fillWidth: true; placeholderText: "Name (optional)"; Accessible.name: "Resource name"; selectByMouse: true }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: resourceTarget; objectName: "resourceTarget"; Layout.fillWidth: true; selectByMouse: true
                    placeholderText: resourceDialog.kind === "file" ? "File path" : "https://example.com"
                    Accessible.name: resourceDialog.kind === "file" ? "File path" : "Web address"
                    onAccepted: resourceDialog.saveResource()
                }
                Button { visible: resourceDialog.kind === "file"; text: "Browse…"; onClicked: filePicker.open() }
            }
            Label { visible: resourceDialog.kind === "file"; text: "The file is linked, not embedded. Move it with the mind map to keep the reference available."; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
            Label { text: controller.error; visible: text.length>0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: ShellTheme.colors["#f08b83"] || "#f08b83" }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; onClicked: resourceDialog.close() }
                Button { objectName: "saveResource"; text: resourceDialog.resourceIndex<0 ? "Add" : "Save"; highlighted: true; enabled: resourceTarget.text.trim().length>0; onClicked: resourceDialog.saveResource() }
            }
        }
        function saveResource() {
            if(controller.setResource(nodeId,resourceIndex,kind,resourceName.text,resourceTarget.text)) close()
        }
    }
    FileDialog {
        id: filePicker; title: "Choose a linked file"; fileMode: FileDialog.OpenFile
        onAccepted: resourceTarget.text=selectedFile.toString()
    }
}
