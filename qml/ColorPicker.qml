import QtQuick
import Mindarchy 1.0
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: picker
    objectName: "appColorPicker"
    parent: Overlay.overlay
    modal: true
    width: parent ? Math.max(0, Math.min(360, parent.width - 32)) : 360
    x: parent ? (parent.width-width)/2 : 0; y: parent ? (parent.height-height)/2 : 0
    padding: 20
    property real hue: 0
    property real saturation: 0
    property real brightness: 1
    property real alpha: 1
    readonly property color selectedColor: Qt.hsva(hue, saturation, brightness, alpha)
    readonly property bool validHex: /^#([0-9a-f]{6}|[0-9a-f]{8})$/i.test(hex.text)
    function choose(color) {
        var value = Qt.color(color)
        hue = Math.max(0, value.hsvHue)
        saturation = value.hsvSaturation
        brightness = value.hsvValue
        alpha = value.a
        hex.text = selectedColor.toString()
    }
    function openColor(color) { choose(color); open() }
    onSelectedColorChanged: hex.text = selectedColor.toString()
    background: Rectangle { color: (ShellTheme.colors["#17232c"] || "#17232c"); radius: 12; border.color: (ShellTheme.colors["#40545f"] || "#40545f") }
    header: Label { text: picker.title; color: (ShellTheme.colors["#e0e9ee"] || "#e0e9ee"); font.pixelSize: 17; font.bold: true; padding: 20; bottomPadding: 0 }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "PRESETS"; color: (ShellTheme.colors["#94a9b7"] || "#94a9b7"); font.pixelSize: 10; font.letterSpacing: 1.3 }
        GridLayout {
            columns: 8; rowSpacing: 6; columnSpacing: 6; Layout.fillWidth: true
            Repeater {
                model: ["#ffffff", "#e0e9ee", "#a5b5bf", "#718896", "#473c32", "#253540", "#17232c", "#000000",
                        "#ef8585", "#f3ac79", "#f3ddb4", "#e5cc69", "#9fc978", "#36b879", "#70d8c4", "#3da995",
                        "#83c8e5", "#669dda", "#9fafd3", "#858bdd", "#aca4dc", "#be8bc9", "#dc9cbe", "#e4c9c0"]
                Button {
                    required property string modelData
                    objectName: "preset-" + modelData.substring(1)
                    Layout.fillWidth: true; implicitHeight: 28; implicitWidth: 28
                    Accessible.name: "Color " + modelData
                    onClicked: picker.choose(modelData)
                    background: Rectangle {
                        color: parent.modelData; radius: 5
                        border.width: parent.hovered || parent.activeFocus || Qt.colorEqual(picker.selectedColor, parent.modelData) ? 2 : 1
                        border.color: parent.hovered || parent.activeFocus ? (ShellTheme.colors["#70d8c4"] || "#70d8c4") : (ShellTheme.colors["#627782"] || "#627782")
                    }
                    ToolTip.visible: hovered; ToolTip.text: modelData
                }
            }
        }
        Label { text: "CUSTOM COLOR"; color: (ShellTheme.colors["#94a9b7"] || "#94a9b7"); font.pixelSize: 10; font.letterSpacing: 1.3 }
        Rectangle {
            id: colorArea; objectName: "colorSpectrum"
            Layout.fillWidth: true; implicitHeight: 130
            color: Qt.hsva(picker.hue, 1, 1, 1)
            activeFocusOnTab: true
            Accessible.name: "Saturation and brightness. Use arrow keys to adjust."
            Rectangle { anchors.fill: parent; gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "white" } GradientStop { position: 1; color: "transparent" } } }
            Rectangle { anchors.fill: parent; gradient: Gradient {
                GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "black" } } }
            Rectangle {
                x: picker.saturation * parent.width - 5; y: (1-picker.brightness) * parent.height - 5
                width: 10; height: 10; radius: 5; color: "transparent"; border.color: "white"; border.width: 2
            }
            function adjust(x, y) {
                picker.saturation = Math.max(0, Math.min(1, x/width))
                picker.brightness = 1-Math.max(0, Math.min(1, y/height))
            }
            MouseArea {
                anchors.fill: parent
                onPressed: function(mouse) { colorArea.forceActiveFocus(); colorArea.adjust(mouse.x, mouse.y) }
                onPositionChanged: function(mouse) { if (pressed) colorArea.adjust(mouse.x, mouse.y) }
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Left) picker.saturation = Math.max(0, picker.saturation-.01)
                else if (event.key === Qt.Key_Right) picker.saturation = Math.min(1, picker.saturation+.01)
                else if (event.key === Qt.Key_Up) picker.brightness = Math.min(1, picker.brightness+.01)
                else if (event.key === Qt.Key_Down) picker.brightness = Math.max(0, picker.brightness-.01)
                else return
                event.accepted = true
            }
        }
        Slider {
            id: hueSlider; objectName: "colorHue"; Layout.fillWidth: true
            from: 0; to: 1; value: picker.hue
            Accessible.name: "Hue"
            onMoved: picker.hue = value
            background: Rectangle {
                x: hueSlider.leftPadding; y: (hueSlider.height-height)/2
                width: hueSlider.availableWidth; height: 10; radius: 5
                gradient: Gradient { orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#ff0000" } GradientStop { position: 1/6; color: "#ffff00" }
                    GradientStop { position: 2/6; color: "#00ff00" } GradientStop { position: 3/6; color: "#00ffff" }
                    GradientStop { position: 4/6; color: "#0000ff" } GradientStop { position: 5/6; color: "#ff00ff" }
                    GradientStop { position: 1; color: "#ff0000" } }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Opacity"; color: (ShellTheme.colors["#d5e1e9"] || "#d5e1e9") }
            Slider { objectName: "colorOpacity"; Layout.fillWidth: true; from: 0; to: 1; value: picker.alpha
                Accessible.name: "Opacity"; onMoved: picker.alpha = value }
            Label { text: Math.round(picker.alpha*100) + "%"; color: (ShellTheme.colors["#94a9b7"] || "#94a9b7"); Layout.preferredWidth: 36 }
        }
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                implicitWidth: 36; implicitHeight: 36; color: (ShellTheme.colors["#c3cbd0"] || "#c3cbd0"); radius: 5
                Rectangle { anchors.fill: parent; color: picker.selectedColor; radius: 5; border.color: (ShellTheme.colors["#718896"] || "#718896") }
            }
            TextField {
                id: hex; objectName: "colorHex"; Layout.fillWidth: true; selectByMouse: true
                Accessible.name: "Hex color, RRGGBB or AARRGGBB"
                placeholderText: "#RRGGBB or #AARRGGBB"
                color: picker.validHex ? (ShellTheme.colors["#e0e9ee"] || "#e0e9ee") : (ShellTheme.colors["#ef8585"] || "#ef8585")
                onTextEdited: { if (picker.validHex) picker.choose(text) }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { objectName: "colorTransparent"; text: "Transparent"; onClicked: picker.alpha = 0 }
            Item { Layout.fillWidth: true }
            Button { objectName: "colorCancel"; text: "Cancel"; onClicked: picker.reject() }
            Button { objectName: "colorApply"; text: "Apply"; highlighted: true; enabled: picker.validHex; onClicked: picker.accept() }
        }
    }
}
