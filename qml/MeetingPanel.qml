import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var controller
    required property var commitEditor
    readonly property bool meeting: controller.selectedMeeting.date !== undefined
    spacing: 8
    Label { text: panel.meeting ? "MEETING DETAILS" : "TEMPLATE"; color: "#94a9b7"; font.pixelSize: 10; font.letterSpacing: 1.5 }
    Button {
        objectName: "applyMeetingTemplate"; visible: !panel.meeting; Layout.fillWidth: true
        text: "Meeting Notes"; enabled: controller.selection.length===1
        onClicked: { if(panel.commitEditor("")) controller.applyMeetingTemplate() }
    }
    ColumnLayout {
        visible: panel.meeting; Layout.fillWidth: true; spacing: 8
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: date; objectName: "meetingDate"; Layout.fillWidth: true; Layout.preferredWidth: 130
                text: controller.selectedMeeting.date || ""; placeholderText: "YYYY-MM-DD"; selectByMouse: true
                Accessible.name: "Meeting date"
                onTextEdited: panel.applyDetails()
            }
            TextField {
                id: time; objectName: "meetingTime"; Layout.preferredWidth: 70
                text: controller.selectedMeeting.time || ""; placeholderText: "HH:mm"; selectByMouse: true
                Accessible.name: "Meeting time (optional)"
                onTextEdited: panel.applyDetails()
            }
        }
        TextField {
            id: attendees; objectName: "meetingAttendees"; Layout.fillWidth: true
            text: controller.selectedMeeting.attendees || ""; placeholderText: "Attendees"; selectByMouse: true
            Accessible.name: "Meeting attendees"
            onTextEdited: panel.applyDetails()
        }
    }
    function applyDetails() {
        if (/^\d{4}-\d{2}-\d{2}$/.test(date.text) && (time.text==="" || /^\d{2}:\d{2}$/.test(time.text)))
            controller.updateMeeting(date.text,time.text,attendees.text)
    }
}
