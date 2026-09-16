import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mindarchy 1.0

Popup {
    id: dialog
    objectName: "nodeTemplateDialog"
    required property var controller
    signal templateAdded()
    property string chosenId: ""
    readonly property bool needsWeek: chosenId === "weekly-tasks"
    property var month: ({})
    property string selectedMonday: ""
    property string selectedLabel: ""
    readonly property color ink: ShellTheme.colors["#dce8ef"] || "#dce8ef"
    readonly property color muted: ShellTheme.colors["#94a9b7"] || "#94a9b7"
    readonly property color accent: ShellTheme.colors["#56d6c9"] || "#56d6c9"
    focus: true
    padding: 8
    width: needsWeek ? 360 : 248
    y: (parent ? parent.height : 36) + 4
    x: parent ? Math.round((parent.width - width) / 2) : 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    onOpened: {
        chosenId = ""
        month = controller.templateCalendar("")
        selectedMonday = month.currentMonday || ""
        selectedLabel = ""
        for (var week of month.weeks || []) if (week.monday === selectedMonday) selectedLabel = week.label
    }
    onClosed: chosenId = ""
    function moveMonth(offset) {
        var next = controller.templateCalendar(month.anchor, offset)
        if (next.anchor !== undefined) month = next
    }
    function chooseTemplate(id) {
        if (id === "meeting-notes") {
            dialog.close()
            if (controller.addNodeTemplate(id, "")) dialog.templateAdded()
            return
        }
        chosenId = id
        month = controller.templateCalendar("")
        selectedMonday = month.currentMonday || ""
        selectedLabel = ""
        for (var week of month.weeks || []) if (week.monday === selectedMonday) selectedLabel = week.label
    }
    background: Rectangle {
        radius: 10
        color: dialog.parent && dialog.parent.Window.window ? dialog.parent.Window.window.palette.window : (ShellTheme.colors["#172129"] || "#172129")
        border.color: (ShellTheme.colors["#34434c"] || "#34434c")
    }
    contentItem: Column {
        spacing: 6
        Repeater {
            model: controller.nodeTemplates
            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool otherChosen: dialog.chosenId.length > 0 && dialog.chosenId !== modelData.id
                readonly property bool showWeek: dialog.needsWeek && modelData.id === "weekly-tasks"
                width: parent.width
                clip: true
                opacity: otherChosen ? 0 : 1
                height: otherChosen ? 0 : body.height
                visible: height > 0.5 || !otherChosen
                Behavior on opacity { NumberAnimation { duration: 160 } }
                Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                Column {
                    id: body
                    width: parent.width
                    spacing: 6
                    Button {
                        id: option
                        objectName: modelData.id === "weekly-tasks" ? "nodeTemplateChoice" : "templateOption-" + modelData.id
                        width: parent.width
                        implicitHeight: 36
                        hoverEnabled: true
                        enabled: !row.otherChosen
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData.name
                        Accessible.description: modelData.description || ""
                        checked: dialog.chosenId === modelData.id
                        onClicked: dialog.chooseTemplate(modelData.id)
                        background: Rectangle {
                            radius: 7
                            color: option.down ? (ShellTheme.colors["#35505a"] || "#35505a")
                                : option.checked ? (ShellTheme.colors["#29463f"] || "#29463f")
                                : option.hovered || option.activeFocus ? (ShellTheme.colors["#2a3d48"] || "#2a3d48")
                                : (ShellTheme.colors["#22313b"] || "#22313b")
                            border.width: option.checked || option.activeFocus ? 1 : 0
                            border.color: dialog.accent
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Image {
                                source: "qrc:/qml/icons/" + (row.modelData.icon || "calendar-week") + ".svg"
                                sourceSize: Qt.size(18, 18)
                                Layout.preferredWidth: 18; Layout.preferredHeight: 18
                            }
                            Label { text: row.modelData.name; color: dialog.ink; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                    }
                    Item {
                        width: parent.width
                        clip: true
                        height: row.showWeek ? weekBody.height : 0
                        opacity: row.showWeek ? 1 : 0
                        visible: height > 0.5
                        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        Behavior on opacity { NumberAnimation { duration: 180 } }
                        ColumnLayout {
                            id: weekBody
                            width: parent.width
                            spacing: 6
                            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: (ShellTheme.colors["#2a3943"] || "#2a3943") }
                            RowLayout {
                                Layout.fillWidth: true
                                ToolButton {
                                    icon.source: "qrc:/qml/icons/chevron-left.svg"; icon.color: dialog.ink; icon.width: 16; icon.height: 16
                                    Accessible.name: "Previous month"; onClicked: dialog.moveMonth(-1)
                                    display: AbstractButton.IconOnly
                                }
                                Label { text: dialog.month.title || ""; font.bold: true; color: dialog.ink; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                                ToolButton {
                                    icon.source: "qrc:/qml/icons/chevron-right.svg"; icon.color: dialog.ink; icon.width: 16; icon.height: 16
                                    Accessible.name: "Next month"; onClicked: dialog.moveMonth(1)
                                    display: AbstractButton.IconOnly
                                }
                                Button {
                                    text: "Today"; implicitHeight: 28
                                    onClicked: {
                                        dialog.month = controller.templateCalendar("")
                                        dialog.selectedMonday = dialog.month.currentMonday
                                        dialog.selectedLabel = ""
                                        for (var week of dialog.month.weeks) if (week.monday === dialog.selectedMonday) dialog.selectedLabel = week.label
                                    }
                                }
                            }
                            Row {
                                Layout.fillWidth: true
                                Repeater {
                                    model: ["Wk", "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"]
                                    Label { required property string modelData; width: weekBody.width / 8; text: modelData; horizontalAlignment: Text.AlignHCenter; color: dialog.muted; font.pixelSize: 10 }
                                }
                            }
                            Column {
                                objectName: "templateWeekCalendar"
                                visible: row.showWeek
                                Layout.fillWidth: true; spacing: 3
                                Repeater {
                                    model: dialog.month.weeks || []
                                    delegate: Button {
                                        id: weekRow
                                        objectName: "templateWeek_" + modelData.monday
                                        required property var modelData
                                        width: weekBody.width; height: 32
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
                                                width: weekRow.width / 8; height: 32; text: weekRow.modelData.number
                                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                color: dialog.ink; font.bold: true; font.pixelSize: 12
                                            }
                                            Repeater {
                                                model: weekRow.modelData.days
                                                Label {
                                                    required property var modelData
                                                    width: weekRow.width / 8; height: 32; verticalAlignment: Text.AlignVCenter
                                                    horizontalAlignment: Text.AlignHCenter; text: modelData.day; font.pixelSize: 12
                                                    color: modelData.inMonth ? dialog.ink : dialog.muted
                                                    opacity: modelData.inMonth ? 1 : 0.5
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Label { objectName: "templateSelectedWeek"; text: dialog.selectedLabel; Layout.fillWidth: true; wrapMode: Text.Wrap; font.bold: true; color: dialog.ink; font.pixelSize: 12 }
                            Label { visible: controller.error.length > 0; text: controller.error; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#efaa96" }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                Button { text: "Cancel"; implicitHeight: 32; onClicked: dialog.close() }
                                Button {
                                    objectName: "addNodeTemplate"; text: "Add template"; highlighted: true; implicitHeight: 32
                                    enabled: dialog.selectedMonday.length > 0 && controller.selection.length === 1
                                    onClicked: {
                                        const id = dialog.chosenId
                                        const monday = dialog.selectedMonday
                                        dialog.close()
                                        if (controller.addNodeTemplate(id, monday)) dialog.templateAdded()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
