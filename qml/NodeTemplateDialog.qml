import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mindarchy 1.0

Dialog {
    id: dialog
    objectName: "nodeTemplateDialog"
    required property var controller
    signal templateAdded()
    readonly property bool needsWeek: templateChoice.currentValue === "weekly-tasks"
    property var month: ({})
    property string selectedMonday: ""
    property string selectedLabel: ""
    readonly property color ink: ShellTheme.colors["#dce8ef"] || "#dce8ef"
    readonly property color muted: ShellTheme.colors["#94a9b7"] || "#94a9b7"
    readonly property color accent: ShellTheme.colors["#56d6c9"] || "#56d6c9"
    title: "Add node template"
    modal: true
    width: Math.min(490, parent.width - 32)
    anchors.centerIn: parent
    padding: 20
    onOpened: {
        month = controller.templateCalendar("")
        selectedMonday = month.currentMonday
        selectedLabel = ""
        for (var week of month.weeks) if (week.monday === selectedMonday) selectedLabel = week.label
    }
    function moveMonth(offset) {
        var next = controller.templateCalendar(month.anchor, offset)
        if (next.anchor !== undefined) month = next
    }
    contentItem: ColumnLayout {
        spacing: 12
        ComboBox {
            id: templateChoice; objectName: "nodeTemplateChoice"
            Layout.fillWidth: true; model: controller.nodeTemplates
            textRole: "name"; valueRole: "id"
            Accessible.name: "Node template"
        }
        Label {
            text: controller.nodeTemplates[templateChoice.currentIndex]?.description || ""
            color: dialog.muted; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        RowLayout {
            visible: dialog.needsWeek
            Layout.fillWidth: true
            ToolButton { text: "‹"; Accessible.name: "Previous month"; onClicked: dialog.moveMonth(-1) }
            Label { text: dialog.month.title || ""; font.bold: true; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
            ToolButton { text: "›"; Accessible.name: "Next month"; onClicked: dialog.moveMonth(1) }
            Button { text: "Today"; onClicked: { dialog.month = controller.templateCalendar(""); dialog.selectedMonday = dialog.month.currentMonday; for (var week of dialog.month.weeks) if (week.monday === dialog.selectedMonday) dialog.selectedLabel = week.label } }
        }
        Row {
            visible: dialog.needsWeek
            Layout.fillWidth: true
            Repeater {
                model: ["Week", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
                Label { required property string modelData; width: (dialog.availableWidth)/8; text: modelData; horizontalAlignment: Text.AlignHCenter; color: dialog.muted; font.pixelSize: 11 }
            }
        }
        Column {
            objectName: "templateWeekCalendar"
            visible: dialog.needsWeek
            Layout.fillWidth: true; spacing: 4
            Repeater {
                model: dialog.month.weeks || []
                delegate: Button {
                    id: weekRow
                    objectName: "templateWeek_" + modelData.monday
                    required property var modelData
                    width: dialog.availableWidth; height: 36
                    padding: 0
                    hoverEnabled: true
                    Accessible.name: modelData.label
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: dialog.selectedMonday === modelData.monday
                    onClicked: { dialog.selectedMonday = modelData.monday; dialog.selectedLabel = modelData.label }
                    background: Rectangle {
                        radius: 6; color: dialog.accent
                        opacity: dialog.selectedMonday === weekRow.modelData.monday ? 0.2 : weekRow.hovered || weekRow.activeFocus ? 0.08 : 0
                    }
                    contentItem: Row {
                        Label {
                            width: weekRow.width/8; height: 36; text: weekRow.modelData.number
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            color: dialog.ink; font.bold: true
                        }
                        Repeater {
                            model: weekRow.modelData.days
                            Label {
                                required property var modelData
                                width: weekRow.width/8; height: 36; verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter; text: modelData.day
                                color: modelData.inMonth ? dialog.ink : dialog.muted
                                opacity: modelData.inMonth ? 1 : 0.5
                            }
                        }
                    }
                }
            }
        }
        Label { visible: dialog.needsWeek; text: "Click any date or week number to select its week."; color: dialog.muted; font.pixelSize: 12 }
        Label { visible: dialog.needsWeek; objectName: "templateSelectedWeek"; text: dialog.selectedLabel; Layout.fillWidth: true; wrapMode: Text.Wrap; font.bold: true }
        Label { visible: controller.error.length > 0; text: controller.error; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#efaa96" }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Cancel"; onClicked: dialog.close() }
            Button {
                objectName: "addNodeTemplate"; text: "Add template"; highlighted: true
                enabled: (!dialog.needsWeek || dialog.selectedMonday.length > 0) && controller.selection.length === 1
                onClicked: {
                    if (controller.addNodeTemplate(templateChoice.currentValue, dialog.selectedMonday)) {
                        dialog.close(); dialog.templateAdded()
                    }
                }
            }
        }
    }
}
