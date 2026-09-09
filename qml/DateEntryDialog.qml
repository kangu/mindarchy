import QtQuick
import Mindarchy 1.0
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog; objectName: "dateEntryDialog"
    required property var controller
    required property var canvas
    property int nodeId: -1
    property string day: ""
    property bool existing: false
    property string initialText: ""
    readonly property bool entryModified: opened && entry.text !== initialText
    modal: true; closePolicy: Popup.NoAutoClose
    width: Math.min(400, parent.width - 32)
    x: (parent.width-width)/2; y: (parent.height-height)/2
    title: (existing ? "Edit date entry · " : "New date entry · ") + day
    function openEntry(id, date, text) {
        nodeId=id; day=date; existing=text.length>0; entry.text=text; initialText=text; open()
    }
    function saveEntry() {
        if(entry.text.trim().length && controller.setDateEntry(nodeId,day,entry.text)) close()
    }
    onOpened: { entry.forceActiveFocus(); entry.selectAll() }
    onClosed: canvas.forceActiveFocus()
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Shown when this day is hovered."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        TextArea {
            id: entry; objectName: "dateEntryText"; Layout.fillWidth: true; Layout.preferredHeight: 110
            textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; selectByMouse: true
            placeholderText: "Add a detail for this date…"
            Keys.onPressed: function(event) {
                if(inputMethodComposing) return
                if(event.key===Qt.Key_Escape) { dialog.close(); event.accepted=true }
                else if((event.key===Qt.Key_Return || event.key===Qt.Key_Enter) && (event.modifiers & (Qt.ControlModifier|Qt.MetaModifier))) {
                    dialog.saveEntry(); event.accepted=true
                }
            }
        }
        Label { visible: controller.error.length>0; text: controller.error; color: (ShellTheme.colors["#efaa96"] || "#efaa96"); wrapMode: Text.Wrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Button { objectName: "dateEntryRemove"; text: "Remove"; visible: dialog.existing
                onClicked: { if(controller.setDateEntry(dialog.nodeId,dialog.day,"")) dialog.close() } }
            Item { Layout.fillWidth: true }
            Button { objectName: "dateEntryCancel"; text: "Cancel"; onClicked: dialog.close() }
            Button { objectName: "dateEntrySave"; text: "Save"; enabled: entry.text.trim().length>0; onClicked: dialog.saveEntry() }
        }
    }
}
