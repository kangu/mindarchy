import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: panel
    required property var controller
    required property var commitEditor
    readonly property var values: controller.selectedStyle
    readonly property bool bare: values.shape === 3 || values.shape === 6
    spacing: 10
    enabled: values.count > 0
    function mixed(key) { return values.mixed.indexOf(key) >= 0 }
    function apply(key, value) {
        if (!commitEditor("")) return false
        var patch = {}; patch[key] = value
        return controller.applyNodeStyle(patch)
    }
    component Section: Label {
        Layout.fillWidth: true; topPadding: 10
        color: "#94a9b7"; font.pixelSize: 10; font.letterSpacing: 1.5
    }
    component Choice: ColumnLayout {
        required property string label
        required property string field
        required property var choices
        spacing: 4; Layout.fillWidth: true
        Label { text: parent.label + (panel.mixed(parent.field) ? " · Mixed" : ""); color: "#d5e1e9" }
        ComboBox {
            id: combo; objectName: "style-" + parent.field
            Layout.fillWidth: true; model: parent.choices; textRole: "label"; valueRole: "value"
            currentIndex: { var v=panel.values[parent.field]; for(var i=0;i<model.length;i++) if(model[i].value===v) return i; return -1 }
            displayText: panel.mixed(parent.field) ? "Mixed" : currentText
            onActivated: panel.apply(parent.field, currentValue)
        }
    }
    component NumberField: RowLayout {
        required property string label
        required property string field
        property int minimum: 0
        property int maximum: 20
        property int factor: maximum === 20 ? 10 : 1
        Layout.fillWidth: true
        Label { Layout.fillWidth: true; text: parent.label + (panel.mixed(parent.field) ? " · Mixed" : ""); color: "#d5e1e9" }
        SpinBox {
            objectName: "style-" + parent.field; Layout.preferredWidth: 112
            from: parent.minimum * parent.factor; to: parent.maximum * parent.factor; editable: true
            value: Math.round((panel.values[parent.field] || 0) * parent.factor)
            textFromValue: function(value, locale) { return Number(value / parent.factor).toLocaleString(locale, "f", parent.factor === 10 ? 1 : 0) }
            valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * parent.factor) }
            onValueModified: panel.apply(parent.field, value / parent.factor)
        }
    }
    component ColorField: RowLayout {
        id: fieldRow
        required property string label
        required property string field
        Layout.fillWidth: true
        Label { Layout.fillWidth: true; text: fieldRow.label; color: "#d5e1e9" }
        Button {
            objectName: "style-" + fieldRow.field + "-picker"
            implicitWidth: 36; implicitHeight: 32
            Accessible.name: fieldRow.label + " color"
            contentItem: Rectangle { color: panel.values[fieldRow.field] || "transparent"; radius: 4; border.color: "#718896"
                Label { anchors.centerIn: parent; text: panel.mixed(fieldRow.field) ? "…" : ""; color: "white" }
            }
            onClicked: { if (!panel.commitEditor("")) return; picker.selectedColor = panel.values[fieldRow.field]; picker.open() }
        }
        TextField {
            objectName: "style-" + fieldRow.field
            Layout.preferredWidth: 105; selectByMouse: true
            text: panel.mixed(fieldRow.field) ? "" : panel.values[fieldRow.field]
            placeholderText: "Mixed"
            Accessible.name: fieldRow.label + " hex color"
            onEditingFinished: { if (text.length) panel.apply(fieldRow.field, text) }
        }
        ColorDialog { id: picker; title: fieldRow.label + " color"; options: ColorDialog.ShowAlphaChannel
            onAccepted: panel.apply(fieldRow.field, selectedColor.toString()) }
    }
    Section { text: "SHAPE" }
    Choice { label: "Node shape"; field: "shape"; choices: [
        {label:"Line",value:3}, {label:"Embedded",value:6}, {label:"Rectangle",value:1},
        {label:"Rounded",value:0}, {label:"Pill",value:2}, {label:"Cloud",value:5},
        {label:"Hexagon",value:4}, {label:"Octagon",value:7}] }
    CheckBox {
        objectName: "style-fixedWidth"; text: "Fixed width" + (panel.mixed("width") ? " · Mixed" : "")
        checked: panel.values.width > 0
        onClicked: panel.apply("width", checked ? Math.max(70,Math.min(1200,panel.values.actualWidth)) : 0)
    }
    NumberField { label: "Width (px)"; field: "width"; minimum: 70; maximum: 1200; enabled: panel.values.width > 0 }
    ColorField { label: "Fill"; field: "fill"; enabled: !panel.bare }
    Section { text: "BORDER" }
    Choice { label: "Stroke"; field: "borderStyle"; enabled: !panel.bare
        choices: [{label:"Solid",value:1},{label:"Dashed",value:2},{label:"Dotted",value:3}] }
    NumberField { label: "Thickness (px)"; field: "borderWidth"; enabled: !panel.bare }
    ColorField { label: "Border"; field: "border"; enabled: !panel.bare }
    Section { text: "BRANCH" }
    Choice { label: "Stroke"; field: "branchStroke"
        choices: [{label:"Solid",value:1},{label:"Dashed",value:2},{label:"Dotted",value:3}] }
    NumberField { label: "Thickness (px)"; field: "branchWidth" }
    ColorField { label: "Branch"; field: "branch" }
    Section { text: "FONT" }
    ComboBox {
        objectName: "style-fontFamily"; Layout.fillWidth: true; model: controller.fontFamilies; editable: true
        currentIndex: model.indexOf(panel.values.fontFamily)
        displayText: panel.mixed("fontFamily") ? "Mixed fonts" : panel.values.fontFamily
        Accessible.name: "Font family"
        onActivated: panel.apply("fontFamily",currentText)
        onAccepted: panel.apply("fontFamily",editText)
    }
    ComboBox {
        objectName: "style-fontFace"; Layout.fillWidth: true
        model: ["Regular", "Bold", "Italic", "Bold Italic"]
        currentIndex: (panel.values.bold ? 1 : 0) + (panel.values.italic ? 2 : 0)
        displayText: panel.mixed("bold") || panel.mixed("italic") ? "Mixed faces" : currentText
        Accessible.name: "Font face"
        onActivated: { if (panel.commitEditor("")) controller.applyNodeStyle({bold: (currentIndex & 1) !== 0, italic: (currentIndex & 2) !== 0}) }
    }
    NumberField { label: "Size (px)"; field: "fontSize"; minimum: 8; maximum: 144 }
    RowLayout {
        Layout.fillWidth: true
        Repeater {
            model: [{key:"bold",label:"B",name:"Bold"},{key:"italic",label:"I",name:"Italic"},
                    {key:"underline",label:"U",name:"Underline"},{key:"strike",label:"S",name:"Strikethrough"}]
            Button {
                required property var modelData
                objectName: "style-" + modelData.key
                Layout.fillWidth: true; text: modelData.label; checkable: true
                checked: panel.values[modelData.key] && !panel.mixed(modelData.key)
                font.bold: modelData.key==="bold"; font.italic: modelData.key==="italic"
                font.underline: modelData.key==="underline"; font.strikeout: modelData.key==="strike"
                Accessible.name: modelData.name; ToolTip.visible: hovered; ToolTip.text: modelData.name
                onClicked: panel.apply(modelData.key,checked)
            }
        }
    }
    Choice { label: "Alignment"; field: "alignment"; choices: [
        {label:"Left",value:0},{label:"Center",value:1},{label:"Right",value:2},{label:"Justified",value:3}] }
    ColorField { label: "Text"; field: "textColor" }
    Button {
        objectName: "style-reset"; Layout.fillWidth: true; text: "Reset to theme"
        ToolTip.visible: hovered; ToolTip.text: "Clear custom appearance and title formatting on selected nodes"
        onClicked: { if(panel.commitEditor("")) controller.resetNodeStyle() }
    }
}
