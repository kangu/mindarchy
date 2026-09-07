import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MindmapLab 1.0

ApplicationWindow {
    id: window
    width: 1380; height: 900
    minimumWidth: 600; minimumHeight: 640
    visible: true
    readonly property bool integratedMacToolbar: Qt.platform.os === "osx"
    flags: integratedMacToolbar
        ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
        : Qt.Window
    // The toolbar reserves horizontal space for the native window controls.
    // Avoid ApplicationWindow's automatic inset below the macOS title bar.
    Binding { target: window; property: "topPadding"; value: 0; when: window.integratedMacToolbar }
    title: "Mindmap Lab · Qt Quick"
    color: "#111920"
    property var controller: engine
    property bool outlineVisible: width >= 1100
    property bool inspectorVisible: width >= 1000
    property string exportStatus: ""
    readonly property color ink: "#e0e9ee"
    readonly property color muted: "#81939f"
    readonly property color accent: "#70d8c4"
    palette.window: "#172129"
    palette.windowText: ink
    palette.base: "#111b23"
    palette.alternateBase: "#1e2c36"
    palette.text: ink
    palette.button: "#253540"
    palette.buttonText: ink
    palette.highlight: "#317d73"
    palette.highlightedText: "#ffffff"
    font.family: "Sans Serif"
    font.pixelSize: 13

    function localPath(url) {
        var s = decodeURIComponent(url.toString())
        if (s.indexOf("file://") === 0) s = s.substring(7)
        if (Qt.platform.os === "windows" && /^\/[A-Za-z]:/.test(s)) s = s.substring(1)
        return s
    }
    function commitEditor(next) {
        if (!canvas.editing) return true
        if (editor.inputMethodComposing) return false
        if (!canvas.commitEditing(editor.text)) { editor.forceActiveFocus(); return false }
        canvas.forceActiveFocus()
        if (next === "child") controller.addChild()
        if (next === "sibling") controller.addSibling()
        return true
    }
    property bool useThemeLayouts: false
    function applyTheme(id) {
        if (!commitEditor("")) return false
        if (useThemeLayouts) {
            for (var i=0; i<controller.themes.length; ++i) {
                var theme = controller.themes[i]
                if (theme.id === id && theme.recipe.layout) {
                    var accepted = controller.applyThemeRecipe(id)
                    if (accepted) canvas.fit()
                    return accepted
                }
            }
        }
        controller.themeId = id
        return controller.themeId === id
    }

    component SmallButton: Button {
        implicitHeight: 32
        font.pixelSize: 12
        focusPolicy: Qt.NoFocus
        background: Rectangle {
            radius: 6
            color: parent.down ? "#35505a" : parent.hovered ? "#2a3d48" : "#22313b"
            border.color: parent.checked ? window.accent : "#32434d"
            opacity: parent.enabled ? 1 : 0.4
        }
        contentItem: Text { text: parent.text; color: parent.enabled ? window.ink : "#647783"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    component IconButton: Button {
        id: iconButton
        required property string iconName
        implicitWidth: 36; implicitHeight: 36
        padding: 8
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        Accessible.name: text
        ToolTip.visible: hovered || activeFocus
        ToolTip.delay: hovered ? 500 : 0
        ToolTip.text: text
        background: Rectangle {
            radius: 7
            color: iconButton.down ? "#35505a" : iconButton.checked ? "#29463f" : iconButton.hovered ? "#2a3d48" : "transparent"
            border.width: iconButton.activeFocus || iconButton.checked ? 1 : 0
            border.color: window.accent
        }
        contentItem: Image {
            source: "qrc:/qml/icons/" + iconButton.iconName + ".svg"
            sourceSize: Qt.size(24,24)
            fillMode: Image.PreserveAspectFit
            opacity: iconButton.enabled ? 1 : 0.3
        }
    }
    component Caption: Label { color: window.muted; font.pixelSize: 10; font.letterSpacing: 1.3; font.bold: true }
    component Rule: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#2a3943" }

    Shortcut { sequences: [StandardKey.Open]; onActivated: openDialog.open() }
    Shortcut { sequences: [StandardKey.Save]; onActivated: { if (!commitEditor("")) return; saveDialog.open() } }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !editor.activeFocus && !notes.activeFocus; onActivated: controller.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: !editor.activeFocus && !notes.activeFocus; onActivated: controller.redo() }

    FileDialog {
        id: openDialog; title: "Open Mindmap Lab document"; nameFilters: ["Mindmap Lab (*.json)"]
        onAccepted: { if (!window.commitEditor("")) return; if (controller.open(window.localPath(selectedFile))) canvas.fit(); canvas.forceActiveFocus() }
    }
    FileDialog {
        id: saveDialog; title: "Save Mindmap Lab document"; fileMode: FileDialog.SaveFile
        nameFilters: ["Mindmap Lab (*.json)"]; defaultSuffix: "json"
        onAccepted: controller.save(window.localPath(selectedFile))
    }
    FileDialog {
        id: imageDialog; title: "Export current canvas view"; fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]; defaultSuffix: "png"
        onAccepted: { if (!window.commitEditor("")) return; canvas.exportPng(window.localPath(selectedFile)) }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            objectName: "mainToolbar"
            Layout.fillWidth: true; implicitHeight: 60; color: "#19242d"
            MouseArea {
                anchors.fill: parent
                enabled: window.integratedMacToolbar
                acceptedButtons: Qt.LeftButton
                onPressed: window.startSystemMove()
                onDoubleClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            }
            Flickable {
                id: toolbarViewport
                anchors.fill: parent
                anchors.leftMargin: window.integratedMacToolbar && window.visibility !== Window.FullScreen ? 96 : 12
                anchors.rightMargin: 12
                interactive: !window.integratedMacToolbar || contentWidth > width
                contentWidth: toolbarRow.width; contentHeight: height; clip: true
                flickableDirection: Flickable.HorizontalFlick
                RowLayout {
                    id: toolbarRow
                    width: Math.max(toolbarViewport.width, implicitWidth)
                    height: parent.height; spacing: 6
                    Image { source: "qrc:/assets/icons/mindmap-blue-64.png"; sourceSize: Qt.size(64,64); Layout.preferredWidth: 32; Layout.preferredHeight: 32; fillMode: Image.PreserveAspectFit }
                    ColumnLayout {
                        visible: window.width >= 950; spacing: 2; Layout.leftMargin: 6; Layout.rightMargin: 12
                        Label { text: "Mindmap Lab"; font.pixelSize: 16; font.bold: true; color: window.ink }
                        Label { visible: window.width >= 1150; text: "A place to think in branches"; color: window.muted; font.pixelSize: 10 }
                    }
                    IconButton { iconName: "panel-left"; text: "Toggle outline"; checkable: true; checked: window.outlineVisible; onClicked: window.outlineVisible = !window.outlineVisible }
                    IconButton { iconName: "panel-right"; text: "Toggle inspector"; checkable: true; checked: window.inspectorVisible; onClicked: window.inspectorVisible = !window.inspectorVisible }
                    Rectangle { implicitWidth: 1; implicitHeight: 24; color: "#34434c" }
                    IconButton { iconName: "undo-2"; text: "Undo"; enabled: controller.canUndo; onClicked: { if (!window.commitEditor("")) return; controller.undo() } }
                    IconButton { iconName: "redo-2"; text: "Redo"; enabled: controller.canRedo; onClicked: { if (!window.commitEditor("")) return; controller.redo() } }
                    Rectangle { implicitWidth: 1; implicitHeight: 24; color: "#34434c" }
                    IconButton { iconName: "corner-down-right"; text: "Add child"; onClicked: { if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addChild() } }
                    IconButton { iconName: "list-plus"; text: "Add sibling"; onClicked: { if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addSibling() } }
                    IconButton { iconName: "link"; text: "Connect selected nodes"; enabled: controller.selection.length === 2; onClicked: { if (!window.commitEditor("")) return; controller.connectSelection(); canvas.forceActiveFocus() } }
                    IconButton { iconName: controller.selectedFolded ? "unfold-vertical" : "fold-vertical"; text: controller.selectedFolded ? "Expand branch" : "Fold branch"; onClicked: { if (!window.commitEditor("")) return; controller.toggleFold() } }
                    Item { Layout.fillWidth: true }
                    Caption { visible: window.width >= 1400; text: "QT QUICK / C++"; Layout.rightMargin: 8 }
                    Rectangle { implicitWidth: 1; implicitHeight: 24; color: "#34434c" }
                    IconButton { iconName: "folder-open"; text: "Open document"; onClicked: openDialog.open() }
                    IconButton { iconName: "save"; text: "Save document"; onClicked: { if (!window.commitEditor("")) return; saveDialog.open() } }
                    IconButton { iconName: "image-down"; text: "Export canvas as PNG"; onClicked: imageDialog.open() }
                }
            }
        }
        Rectangle {
            visible: controller.error.length > 0
            Layout.fillWidth: true; implicitHeight: visible ? errorLabel.implicitHeight + 20 : 0; color: "#563832"
            Label { id: errorLabel; anchors.fill: parent; anchors.margins: 10; text: controller.error; color: "#ffd3c7"; wrapMode: Text.Wrap }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                visible: window.outlineVisible; Layout.preferredWidth: 224; Layout.fillHeight: true; color: "#17232c"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 14
                    RowLayout { Layout.fillWidth: true; Caption { text: "DOCUMENT" }
                Item { Layout.fillWidth: true }
                Label { text: controller.nodeCount; color: window.muted; font.pixelSize: 11 } }
                    Label { text: "Branch outline"; color: window.ink; font.pixelSize: 16; font.bold: true }
                    ListView {
                        id: outline; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: controller.outline; spacing: 3
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: outline.width; height: 32; radius: 5
                            color: controller.selectedId === modelData.id ? "#29463f" : rowMouse.containsMouse ? "#22323d" : "transparent"
                            Label { anchors.left: parent.left; anchors.leftMargin: 8 + Math.min(modelData.depth, 7) * 10; anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: (modelData.folded ? "▸ " : "· ") + modelData.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: controller.selectedId === modelData.id ? window.accent : "#a5b5bf"; font.pixelSize: 12 }
                            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; onClicked: function(mouse) { if (!window.commitEditor("")) return; controller.select(modelData.id, !!(mouse.modifiers & Qt.ShiftModifier)); canvas.forceActiveFocus() }
                onDoubleClicked: canvas.beginEdit(modelData.id) }
                        }
                    }
                    Rule {}
                    Label { text: "Tab to grow a branch.\nReturn to add a sibling."; color: window.muted; font.pixelSize: 11; lineHeight: 1.5 }
                }
            }
            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#2a3943" }
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                MindCanvas {
                    id: canvas; objectName: "mindCanvas"; anchors.fill: parent; engine: window.controller; focus: true
                    onCommitRequested: window.commitEditor("")
                    onEditRequested: function(id, text) { editor.text = text; editor.forceActiveFocus(); editor.selectAll() }
                    onExportFinished: function(path, success) { window.exportStatus = success ? "PNG exported" : "PNG export failed"; exportTimer.restart() }
                    Item {
                        id: inlineEditor; objectName: "inlineNodeEditor"
                        visible: canvas.editing
                        x: canvas.editingRect.x; y: canvas.editingRect.y
                        width: canvas.editingRect.width / canvas.zoom
                        height: canvas.editingRect.height / canvas.zoom
                        scale: canvas.zoom; transformOrigin: Item.TopLeft
                        property var nodeAppearance: { controller.themeId; return canvas.appearanceForNode(canvas.editingId) }
                        property real textInset: 15 + (controller.selectedTask ? 20 : 0)
                        Rectangle {
                            anchors.left: parent.left; anchors.bottom: parent.top; anchors.bottomMargin: 5
                            width: formatRow.implicitWidth + 4; height: formatRow.implicitHeight + 4
                            radius: 6; color: "#1c2b35"; border.color: "#34454f"
                            Row {
                            id: formatRow; x: 2; y: 2; spacing: 4
                            IconButton { iconName: "bold"; text: "Bold"; width: 32; font.bold: true; onClicked: canvas.formatText(editor, "bold") }
                            IconButton { iconName: "italic"; text: "Italic"; width: 32; font.italic: true; onClicked: canvas.formatText(editor, "italic") }
                            IconButton { iconName: "underline"; text: "Underline"; width: 32; font.underline: true; onClicked: canvas.formatText(editor, "underline") }
                            }
                        }
                        TextEdit {
                            id: editor; objectName: "titleEditor"
                            x: inlineEditor.textInset
                            y: Math.max(8, (inlineEditor.height - contentHeight) / 2)
                            width: Math.max(20, inlineEditor.width - inlineEditor.textInset - 15)
                            height: Math.max(22, inlineEditor.height - y)
                            clip: true; textMargin: 0
                            textFormat: TextEdit.RichText; wrapMode: TextEdit.Wrap
                            font.family: "sans-serif"; font.pixelSize: 15
                            color: inlineEditor.nodeAppearance.text || "#f1fff9"; selectionColor: "#438b78"
                            onTextChanged: { if (canvas.editing) canvas.updateEditingText(text) }
                            selectByMouse: true; persistentSelection: true
                            Keys.onPressed: function(event) {
                                if (inputMethodComposing) return
                                if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) && [Qt.Key_B, Qt.Key_I, Qt.Key_U].indexOf(event.key) >= 0) {
                                    canvas.formatText(editor, event.key === Qt.Key_B ? "bold" : event.key === Qt.Key_I ? "italic" : "underline")
                                    event.accepted = true
                                    return
                                }
                                if (event.key === Qt.Key_Escape) { event.accepted = true; window.commitEditor("") }
                                else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) { event.accepted = true; window.commitEditor("") }
                                else if (event.key === Qt.Key_Tab) { window.commitEditor("child"); event.accepted = true }
                            }
                        }
                    }
                }
                Row {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 22; spacing: 8
                    Rectangle { width: 6; height: 6; radius: 3; color: window.accent; anchors.verticalCenter: parent.verticalCenter }
                    Label { text: controller.manual ? "MANUAL PLACEMENT" : controller.layout.toUpperCase() + " LAYOUT"; color: window.muted; font.pixelSize: 10; font.letterSpacing: 1.5 }
                }
                Rectangle {
                    anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 20
                    width: zoomControls.implicitWidth + 16; height: 44; radius: 9; color: "#1c2b35"; border.color: "#34454f"
                    Row { id: zoomControls; anchors.centerIn: parent; spacing: 5
                        IconButton { iconName: "zoom-out"; text: "Zoom out"; width: 32; onClicked: canvas.zoomOut() }
                        Label { text: (canvas.zoom * 100).toFixed(canvas.zoom < .1 ? 2 : 0) + "%"; color: window.ink; anchors.verticalCenter: parent.verticalCenter; width: 50; horizontalAlignment: Text.AlignHCenter }
                        IconButton { iconName: "scan"; text: "Actual size (100%)"; onClicked: canvas.resetZoom() }
                        IconButton { iconName: "zoom-in"; text: "Zoom in"; width: 32; onClicked: canvas.zoomIn() }
                        IconButton { iconName: "maximize"; text: "Fit map"; onClicked: canvas.fit() }
                    }
                }
            }
            Rectangle { visible: window.inspectorVisible; Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#2a3943" }
            Rectangle {
                visible: window.inspectorVisible; Layout.preferredWidth: 274; Layout.fillHeight: true; color: "#19252e"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 15
                    Caption { text: "INSPECTOR" }
                    TabBar { id: inspectorTabs; objectName: "inspectorTabs"; Layout.fillWidth: true; TabButton { text: "Map" }
                TabButton { text: "Node" }
                TabButton { text: "Themes"; objectName: "themesTab" } }
                    ScrollView {
                        objectName: "inspectorScroll"
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        contentWidth: availableWidth
                        ColumnLayout {
                            width: parent.width; spacing: 18
                            ColumnLayout {
                                visible: inspectorTabs.currentIndex === 0; Layout.fillWidth: true; spacing: 12
                                Label { text: "Shape your thinking"; color: window.ink; font.pixelSize: 17; font.bold: true }
                                Label { text: "Switch the arrangement without\nchanging the hierarchy."; color: window.muted; font.pixelSize: 12; lineHeight: 1.35 }
                                Rule {}
                                Caption { text: "DIRECTION" }
                                ComboBox { Layout.fillWidth: true; model: ["Horizontal", "Vertical", "Compact"]; currentIndex: model.indexOf(controller.layout); onActivated: { if (!window.commitEditor("")) return; controller.layout = currentText; canvas.forceActiveFocus() } }
                                Caption { text: "SPACING" }
                                ComboBox { enabled: !controller.manual && controller.layout !== "Compact"; Layout.fillWidth: true; model: ["Narrow", "Standard", "Wide"]; currentIndex: model.indexOf(controller.spacing); onActivated: { if (!window.commitEditor("")) return; controller.spacing = currentText } }
                                Caption { text: "CONNECTIONS" }
                                ComboBox { Layout.fillWidth: true; model: ["Rounded", "Angular"]; currentIndex: model.indexOf(controller.branchStyle); onActivated: { if (!window.commitEditor("")) return; controller.branchStyle = currentText } }
                                Rule {}
                                Switch { enabled: controller.layout !== "Compact"; text: "Manual placement"; checked: controller.manual; onClicked: { if (!window.commitEditor("")) return; controller.manual = checked } }
                                Label { Layout.fillWidth: true; text: controller.manual ? "Drag a node to place it freely. Turn off to restore automatic layout." : "Drag a branch onto another node to move it in the hierarchy."; wrapMode: Text.Wrap; color: window.muted; font.pixelSize: 12; lineHeight: 1.4 }
                                Rule {}
                                Caption { text: "TRY THE PROTOTYPE" }
                                Label { Layout.fillWidth: true; text: "01   Grow and fold branches\n02   Try all three layouts\n03   Drag to reorganize\n04   Explore the Themes tab"; color: "#a5b5bf"; font.pixelSize: 12; lineHeight: 1.8 }
                            }
                            ColumnLayout {
                                visible: inspectorTabs.currentIndex === 2; Layout.fillWidth: true; spacing: 12
                                Label { text: "Choose a theme"; color: window.ink; font.pixelSize: 17; font.bold: true }
                                Label { Layout.fillWidth: true; text: "Apply colors and node styles, or include a suggested layout for the new themes."; wrapMode: Text.Wrap; color: window.muted; font.pixelSize: 12 }
                                CheckBox { objectName: "useThemeLayouts"; text: "Use suggested layout"; checked: window.useThemeLayouts
                                    onClicked: window.useThemeLayouts = checked
                                    ToolTip.visible: hovered; ToolTip.text: "New themes include an automatic layout recipe. Applies with the theme as one undoable change." }
                                Repeater {
                                    model: controller.themes
                                    delegate: ThemeCard {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        theme: modelData
                                        checked: controller.themeId === modelData.id
                                        onClicked: window.applyTheme(modelData.id)
                                    }
                                }
                            }
                            ColumnLayout {
                                visible: inspectorTabs.currentIndex === 1; Layout.fillWidth: true; spacing: 12
                                Caption { text: controller.selection.length === 1 ? "SELECTED NODE · " + controller.selectedId : controller.selection.length + " NODES SELECTED" }
                                NodeStylePanel { Layout.fillWidth: true; controller: window.controller; commitEditor: window.commitEditor }
                                SmallButton { text: "Edit title"; Layout.fillWidth: true; onClicked: { if (!window.commitEditor("")) return; canvas.editSelected() } }
                                Rule {}
                                CheckBox { text: "Task"; checked: controller.selectedTask; onClicked: { if (!window.commitEditor("")) return; controller.toggleTask() } }
                                CheckBox { text: "Completed"; enabled: controller.selectedTask; checked: controller.selectedChecked; onClicked: { if (!window.commitEditor("")) return; controller.toggleChecked() } }
                                SmallButton { text: controller.selectedFolded ? "Expand branch" : "Fold branch"; Layout.fillWidth: true; onClicked: { if (!window.commitEditor("")) return; controller.toggleFold() } }
                                Rule {}
                                Caption { text: "NOTES" }
                                TextArea { id: notes; objectName: "notesEditor"; enabled: controller.selection.length === 1; Layout.fillWidth: true; Layout.preferredHeight: 150; wrapMode: TextEdit.Wrap; property int loadedId: -1
                                    property string loadedNotes: ""
                                    function syncNotes() {
                                        if (loadedId !== controller.selectedId || loadedNotes !== controller.selectedNotes) {
                                            loadedId = controller.selectedId
                                            loadedNotes = controller.selectedNotes
                                            text = loadedNotes
                                        }
                                    }
                                    Component.onCompleted: syncNotes()
                                    Connections { target: controller; function onChanged() { notes.syncNotes() } }
                                    placeholderText: "Capture a detail…"; selectByMouse: true }
                                SmallButton { text: "Apply notes"; enabled: controller.selection.length === 1; Layout.fillWidth: true; onClicked: { controller.setNotes(notes.text); canvas.forceActiveFocus() } }
                                SmallButton { text: "Delete selected branch"; Layout.fillWidth: true; onClicked: { if (!window.commitEditor("")) return; controller.removeSelected(); canvas.forceActiveFocus() } }
                            }
                        }
                    }
                    Rule {}
                    Label { text: "LOCAL DOCUMENT  ·  JSON"; color: window.muted; font.pixelSize: 9; font.letterSpacing: 0.8 }
                }
            }
        }
    }
    // Export feedback remains available after removing the statistics footer.
    Rectangle {
        visible: window.exportStatus.length > 0
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 80
        width: exportMessage.implicitWidth + 28; height: 38; radius: 8; color: "#29463f"
        Label { id: exportMessage; anchors.centerIn: parent; text: window.exportStatus; color: window.ink }
    }
    onWidthChanged: resizeTimer.restart()
    onHeightChanged: resizeTimer.restart()
    Timer { id: resizeTimer; interval: 100; onTriggered: { if (!canvas.editing) canvas.fit() } }
    Timer { id: exportTimer; interval: 5000; onTriggered: window.exportStatus = "" }
    Component.onCompleted: Qt.callLater(function() { canvas.fit(); canvas.forceActiveFocus() })
}
