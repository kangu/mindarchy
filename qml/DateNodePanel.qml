import QtQuick
import Mindarchy 1.0
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var controller
    required property var canvas
    required property var commitEditor
    readonly property var settings: controller.selectedCalendar
    spacing: 8
    component Action: Button {
        required property string iconName
        implicitWidth: 28; implicitHeight: 32; padding: 6
        hoverEnabled: true
        Accessible.name: text
        ToolTip.visible: hovered || activeFocus
        ToolTip.text: text
        background: Rectangle {
            radius: 6
            color: parent.down ? (ShellTheme.colors["#35505a"] || "#35505a") : parent.checked ? (ShellTheme.colors["#29463f"] || "#29463f") : parent.hovered ? (ShellTheme.colors["#2a3d48"] || "#2a3d48") : "transparent"
            border.width: parent.checked || parent.activeFocus ? 1 : 0
            border.color: (ShellTheme.colors["#70d8c4"] || "#70d8c4")
        }
        icon.source: "icons/" + iconName + ".svg"
        icon.color: (ShellTheme.colors["#e0e9ee"] || "#e0e9ee")
        display: AbstractButton.IconOnly
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 4
        Label { text: "DATE"; Layout.fillWidth: true; color: (ShellTheme.colors["#94a9b7"] || "#94a9b7"); font.pixelSize: 10; font.letterSpacing: 1.5 }
        Action {
            objectName: "dateNodeWeek"; iconName: "calendar-week"; text: "Week"; checked: panel.settings.view==="week"
            onClicked: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,"week",panel.settings.anchor) }
        }
        Action {
            objectName: "dateNodeMonth"; iconName: "calendar-days"; text: "Month"; checked: panel.settings.view==="month"
            onClicked: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,"month",panel.settings.anchor) }
        }
    }
    Label { text: panel.settings.title; Layout.fillWidth: true; wrapMode: Text.Wrap }
    RowLayout {
        Layout.fillWidth: true; spacing: 2
        TextField {
            objectName: "dateNodeAnchor"; Layout.fillWidth: true; Layout.minimumWidth: 100; Layout.preferredWidth: 100
            implicitHeight: 32; leftPadding: 6; rightPadding: 6; font.pixelSize: 11
            text: panel.settings.anchor; placeholderText: "YYYY-MM-DD"; selectByMouse: true
            Accessible.name: "Date within displayed week or month"
            onEditingFinished: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,panel.settings.view,text) }
        }
        Action { objectName: "dateNodePrevious"; iconName: "chevron-left"; text: "Previous"
            onClicked: { if(panel.commitEditor("")) controller.shiftDateNode(controller.selectedId,-1) } }
        Action { objectName: "dateNodeNext"; iconName: "chevron-right"; text: "Next"
            onClicked: { if(panel.commitEditor("")) controller.shiftDateNode(controller.selectedId,1) } }
        Action { objectName: "dateNodeToday"; iconName: "calendar-today"; text: "Today"
            onClicked: { if(panel.commitEditor("")) controller.configureDateNode(controller.selectedId,panel.settings.view,Qt.formatDate(new Date(),"yyyy-MM-dd")) } }
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 4
        ComboBox {
            id: day; objectName: "dateNodeDay"; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 140
            implicitHeight: 32; font.pixelSize: 11
            model: panel.settings.days; textRole: "label"; valueRole: "date"; Accessible.name: "Calendar day"
        }
        Action { text: "Edit selected day"; iconName: "pencil"; objectName: "dateNodeEditDay"; enabled: day.currentIndex>=0
            onClicked: { if(panel.commitEditor("")) canvas.editDateEntry(controller.selectedId,day.currentValue) } }
    }
}
