import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var controller
    required property var canvas
    required property var commitEditor
    readonly property var settings: controller.selectedCalendar
    spacing: 10
    Label { text: "DATE"; color: "#94a9b7"; font.pixelSize: 10; font.letterSpacing: 1.5 }
    ComboBox {
        objectName: "dateNodeView"; Layout.fillWidth: true; model: ["Week", "Month"]
        currentIndex: panel.settings.view==="month" ? 1 : 0
        onActivated: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,currentIndex===0 ? "week" : "month",panel.settings.anchor) }
    }
    Label { text: panel.settings.title; Layout.fillWidth: true; wrapMode: Text.Wrap }
    RowLayout {
        Layout.fillWidth: true
        Button { text: "Previous"; Layout.fillWidth: true; onClicked: { if(panel.commitEditor("")) controller.shiftDateNode(controller.selectedId,-1) } }
        Button { text: "Next"; Layout.fillWidth: true; onClicked: { if(panel.commitEditor("")) controller.shiftDateNode(controller.selectedId,1) } }
    }
    TextField {
        objectName: "dateNodeAnchor"; Layout.fillWidth: true; text: panel.settings.anchor; placeholderText: "YYYY-MM-DD"; selectByMouse: true
        Accessible.name: "Date within displayed week or month"
        onEditingFinished: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,panel.settings.view,text) }
    }
    Button { text: "Today"; Layout.fillWidth: true
        onClicked: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,panel.settings.view,Qt.formatDate(new Date(),"yyyy-MM-dd")) } }
    Label { text: "Click a day to add or edit its value. Filled days have an entry; hover to read it. Drag the heading or a day to move the node."; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#94a9b7" }
    ComboBox { id: day; objectName: "dateNodeDay"; Layout.fillWidth: true; model: panel.settings.days; textRole: "label"; valueRole: "date"; Accessible.name: "Calendar day" }
    Button { text: "Edit selected day"; objectName: "dateNodeEditDay"; Layout.fillWidth: true; enabled: day.currentIndex>=0
        onClicked: { if(panel.commitEditor("")) canvas.editDateEntry(controller.selectedId,day.currentValue) } }
}
