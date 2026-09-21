import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Mindarchy 1.0

FocusScope {
    id: window
    objectName: "documentWorkspace"
    required property var hostWindow
    anchors.fill: parent
    implicitWidth: parent ? parent.width : 0
    implicitHeight: parent ? parent.height : 0
    readonly property bool welcomeVisible: hostWindow.welcomeVisible
    property var controller: engine
    readonly property bool integratedMacToolbar: hostWindow.integratedMacToolbar
    readonly property bool integratedWindowsToolbar: hostWindow.integratedWindowsToolbar
    readonly property bool integratedToolbar: hostWindow.integratedToolbar
    readonly property real windowsCaptionWidth: hostWindow.windowsCaptionWidth
    property alias desktopMenuSet: desktopMenus
    function hideWindowsMenu(restoreFocus) { hostWindow.hideWindowsMenu(restoreFocus) }
    function openDocumentMenu() { openDialog.open() }
    DesktopMenus {
        id: desktopMenus; host: window.hostWindow; controller: window.controller; shortcutsWindow: windowsShortcuts
        onCommandChosen: { window.hideWindowsMenu(true); applicationMenu.close() }
        onMenusClosed: Qt.callLater(function() { if (!desktopMenus.popupOpen && Qt.platform.os === "windows") window.hideWindowsMenu(false) })
    }
    KeyboardShortcuts { id: windowsShortcuts; controller: window.controller; transientParent: window.hostWindow }
    readonly property var documentTabs: hostWindow.documentTabs.length ? hostWindow.documentTabs : [{id: documentTabId, title: controller.documentName, edited: documentEdited}]
    property double documentTabId: 0
    readonly property bool canMergeWindows: hostWindow.canMergeWindows
    readonly property bool outlineVisible: hostWindow.outlineVisible && width >= 545
    readonly property bool inspectorVisible: hostWindow.inspectorVisible && width >= 595 + (hostWindow.outlineVisible ? 225 : 0)
    readonly property bool modalInteraction: shareDialog.visible || closeDialog.visible || dateDialog.visible || nodeTemplateDialog.visible || openDialog.visible || saveDialog.visible || imageDialog.visible || quitPending
    readonly property bool modalTabBlocked: modalInteraction
    function commitForTabSwitch() { return !modalInteraction && commitEditor("") }
    function focusDocument() { canvas.forceActiveFocus() }
    function startWelcomeMap() { canvas.fit(); canvas.beginEdit(1) }
    signal closeApproved()
    signal closeCancelled()
    property string exportStatus: ""
    readonly property bool documentEdited: controller.edited ||
        (canvas.editing && editor.text !== editor.initialText) ||
        (notes.loadedId === controller.selectedId && notes.text !== notes.loadedNotes) || dateDialog.entryModified
    property bool allowClose: false
    property bool closeAfterSave: false
    property bool quitPending: false
    property bool forgetOnClose: false
    function recoveryDraft() {
        return {
            selectedId: controller.selectedId,
            editingId: canvas.editing ? canvas.editingId : -1,
            text: canvas.editing ? editor.text : "",
            cursor: canvas.editing ? editor.cursorPosition : 0,
            notesId: notes.loadedId, notes: notes.text,
            date: dateDialog.recoveryDraft(),
            outline: hostWindow.outlineVisible, inspector: hostWindow.inspectorVisible
        }
    }
    function restoreRecoveryDraft(state) {
        if (state.selectedId > 0) controller.select(state.selectedId)
        if (state.outline !== undefined) hostWindow.outlineVisible = state.outline
        if (state.inspector !== undefined) hostWindow.inspectorVisible = state.inspector
        if (state.editingId > 0) {
            canvas.beginEdit(state.editingId)
            editor.text = state.text
            editor.cursorPosition = Math.min(state.cursor, editor.length)
        }
        if (state.notesId === notes.loadedId) notes.text = state.notes
        if (state.date) dateDialog.restoreRecoveryDraft(state.date)
    }
    function prepareRecoveryQuit() {
        Qt.inputMethod.commit()
        quitPending = true
        window.enabled = false
    }
    function requestClose(forget, quitting) {
        if (quitPending) return
        quitPending = quitting
        forgetOnClose = forget
        if (!commitEditor("")) { cancelClose(); return }
        if (notes.loadedId === controller.selectedId && notes.text !== notes.loadedNotes)
            controller.setNotes(notes.text)
        if (controller.hasUnsavedChanges()) {
            if (typeof nativeCloseAvailable !== "undefined" && nativeCloseAvailable)
                controller.nativeCloseRequested()
            else closeDialog.open()
        } else approveClose()
    }
    function approveClose() {
        closeDialog.close()
        if (quitPending) {
            window.enabled = false
            controller.quitDecision(true)
        } else {
            controller.windowCloseApproved(forgetOnClose)
            allowClose = true
            Qt.callLater(function() { window.closeApproved() })
        }
    }
    function cancelClose() {
        closeDialog.close()
        if (quitPending) controller.quitDecision(false)
        quitPending = false
        forgetOnClose = false
        window.enabled = true
        closeCancelled()
    }
    function abortSessionQuit() {
        quitPending = false
        window.enabled = true
    }
    function completeSessionQuit() {
        allowClose = true
        window.closeApproved()
    }
    function saveDocument(closing) {
        if (!commitEditor("")) { if (closing) cancelClose(); return }
        if (notes.loadedId === controller.selectedId && notes.text !== notes.loadedNotes)
            controller.setNotes(notes.text)
        if (controller.documentPath().length > 0) {
            var saved = controller.save(controller.documentPath())
            if (closing) { if (saved) approveClose(); else cancelClose() }
        } else {
            closeAfterSave = closing
            if (typeof nativeCloseAvailable !== "undefined" && nativeCloseAvailable)
                controller.nativeSaveRequested()
            else saveDialog.open()
        }
    }
    function saveBeforeClosing() {
        closeDialog.close()
        saveDocument(true)
    }
    function finishSaveDialog(path) {
        var shouldClose = closeAfterSave
        closeAfterSave = false
        if (!path.length) { if (shouldClose) cancelClose(); return }
        var saved = controller.save(path)
        if (shouldClose) { if (saved) approveClose(); else cancelClose() }
    }
    readonly property color ink: (ShellTheme.colors["#e0e9ee"] || "#e0e9ee")
    readonly property color muted: (ShellTheme.colors["#81939f"] || "#81939f")
    readonly property color accent: (ShellTheme.colors["#70d8c4"] || "#70d8c4")
    function localPath(url) {
        var s = decodeURIComponent(url.toString())
        if (s.indexOf("file://") === 0) s = s.substring(7)
        if (Qt.platform.os === "windows" && /^\/[A-Za-z]:/.test(s)) s = s.substring(1)
        return s
    }
    function commitEditor(next) {
        if (dateDialog.opened) return false
        if (!canvas.editing) return true
        if (editor.inputMethodComposing) return false
        // A rich-text editor serializes plain text as HTML even without user edits.
        if (editor.text === editor.initialText) canvas.endEdit()
        else if (!canvas.commitEditing(editor.text)) { editor.forceActiveFocus(); return false }
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
            color: parent.down ? (ShellTheme.colors["#35505a"] || "#35505a") : parent.hovered ? (ShellTheme.colors["#2a3d48"] || "#2a3d48") : (ShellTheme.colors["#22313b"] || "#22313b")
            border.color: parent.checked ? window.accent : (ShellTheme.colors["#32434d"] || "#32434d")
            opacity: parent.enabled ? 1 : 0.4
        }
        contentItem: Text { text: parent.text; color: parent.enabled ? window.ink : (ShellTheme.colors["#647783"] || "#647783"); font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    component IconButton: Button {
        id: iconButton
        required property string iconName
        property bool showFocusFeedback: true
        implicitWidth: 36; implicitHeight: 36
        padding: 8
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        Accessible.name: text
        ToolTip.visible: hovered || (showFocusFeedback && activeFocus)
        ToolTip.delay: hovered ? 500 : 0
        ToolTip.text: text
        background: Rectangle {
            radius: 7
            color: iconButton.down ? (ShellTheme.colors["#35505a"] || "#35505a") : iconButton.checked ? (ShellTheme.colors["#29463f"] || "#29463f") : iconButton.hovered ? (ShellTheme.colors["#2a3d48"] || "#2a3d48") : "transparent"
            border.width: (iconButton.showFocusFeedback && iconButton.activeFocus) || iconButton.checked ? 1 : 0
            border.color: window.accent
        }
        icon.source: iconButton.iconName.length ? "qrc:/qml/icons/" + iconButton.iconName + ".svg" : ""
        icon.color: window.ink
        icon.width: 24; icon.height: 24
        display: AbstractButton.IconOnly
    }
    component ZoomMenuItem: MenuItem {
        width: parent.width
        highlighted: hovered || activeFocus
        icon.color: window.ink
    }
    component ToolbarButton: IconButton {
        focusPolicy: Qt.NoFocus
        showFocusFeedback: false
        implicitWidth: window.width < 800 ? 32 : 36
        implicitHeight: implicitWidth
    }
    component MapOptionBar: Rectangle {
        id: bar
        required property var options
        required property string selectedValue
        property string optionPrefix: "map-option-"
        signal chosen(string value)
        Layout.fillWidth: true
        implicitHeight: 42
        radius: 8; color: (ShellTheme.colors["#122029"] || "#122029"); border.color: (ShellTheme.colors["#2a3943"] || "#2a3943")
        RowLayout {
            anchors.fill: parent; anchors.margins: 3; spacing: 3
            Repeater {
                id: mapOptions
                model: bar.options
                delegate: IconButton {
                    required property var modelData
                    required property int index
                    objectName: bar.optionPrefix + modelData.value
                    Layout.fillWidth: true; Layout.fillHeight: true
                    iconName: ""; text: modelData.label
                    checked: bar.selectedValue === modelData.value
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: checked
                    onClicked: bar.chosen(modelData.value)
                    Keys.onLeftPressed: { if(index>0) { mapOptions.itemAt(index-1).forceActiveFocus(); bar.chosen(bar.options[index-1].value) } }
                    Keys.onRightPressed: { if(index+1<bar.options.length) { mapOptions.itemAt(index+1).forceActiveFocus(); bar.chosen(bar.options[index+1].value) } }
                    contentItem: Image {
                        source: "data:image/svg+xml," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="' + window.ink + '" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="' + modelData.path + '"/></svg>')
                        sourceSize: Qt.size(24,24); fillMode: Image.PreserveAspectFit
                        opacity: bar.enabled ? 1 : 0.3
                    }
                }
            }
        }
    }
    component Caption: Label { color: window.muted; font.pixelSize: 10; font.letterSpacing: 1.3; font.bold: true }
    component Rule: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: (ShellTheme.colors["#2a3943"] || "#2a3943") }

    property bool searchOpen: false
    property int searchIndex: -1
    property string searchQuery: ""
    readonly property var searchResults: { controller.outline; return controller.searchNodes(window.searchQuery) }
    onSearchResultsChanged: { if (searchIndex >= searchResults.length) searchIndex = -1 }
    function openSearch() {
        if (!window.commitEditor("")) return
        searchOpen = true
        searchField.forceActiveFocus(); searchField.selectAll()
    }
    Timer {
        id: searchDelay; interval: 300
        onTriggered: {
            if (!window.searchOpen) return
            window.searchQuery=searchField.text
            window.searchIndex=-1
            window.nextSearchResult()
        }
    }
    function nextSearchResult() {
        searchDelay.stop()
        if (searchQuery !== searchField.text) { searchQuery=searchField.text; searchIndex=-1 }
        if (!searchResults.length) { canvas.clearSearchHighlight(); return }
        searchIndex = (searchIndex + 1) % searchResults.length
        canvas.focusSearchResult(searchResults[searchIndex], searchQuery)
        searchField.forceActiveFocus()
    }
    Shortcut { enabled: window.visible && window.enabled && (typeof sharedWindowManaged === "undefined" || !sharedWindowManaged); sequences: [StandardKey.Close]; onActivated: window.requestClose(true, false) }
    Shortcut { enabled: window.visible && window.enabled && (typeof sharedWindowManaged === "undefined" || !sharedWindowManaged); sequences: [StandardKey.New]; onActivated: controller.newDocumentRequested() }
    Shortcut { enabled: window.visible && window.enabled; sequences: [StandardKey.Find]; onActivated: window.openSearch() }
    Shortcut { enabled: window.visible && window.enabled; sequences: [StandardKey.Quit]; onActivated: controller.quitRequested() }
    Shortcut { enabled: window.visible && window.enabled; sequences: [StandardKey.Open]; onActivated: openDialog.open() }
    Shortcut { enabled: window.visible && window.enabled; sequences: [StandardKey.Save]; onActivated: window.saveDocument(false) }
    Shortcut { sequences: [StandardKey.Undo]; enabled: window.visible && window.enabled && !editor.activeFocus && !notes.activeFocus && !resourcePanel.editingResource; onActivated: controller.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: window.visible && window.enabled && !editor.activeFocus && !notes.activeFocus && !resourcePanel.editingResource; onActivated: controller.redo() }

    DateEntryDialog { id: dateDialog; controller: window.controller; canvas: canvas; parent: window.hostWindow.Overlay.overlay }

    Dialog {
        id: closeDialog; objectName: "closeConfirmation"
        parent: window.hostWindow.Overlay.overlay
        modal: true; closePolicy: Popup.NoAutoClose
        Shortcut { sequence: "Escape"; enabled: window.visible && window.enabled && closeDialog.opened; onActivated: window.cancelClose() }
        width: Math.min(480, window.width - 40)
        x: (parent.width-width)/2; y: (parent.height-height)/2
        title: "Save changes before closing?"
        contentItem: ColumnLayout {
            spacing: 20
            Image { source: "qrc:/assets/icons/mindarchy-64.png"; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 64; Layout.preferredHeight: 64 }
            Label { text: "Your changes will be lost if you don’t save them."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                Button { objectName: "closeDiscard"; text: "Discard"; palette.buttonText: (ShellTheme.colors["#f08b83"] || "#f08b83"); onClicked: window.approveClose() }
                Item { Layout.fillWidth: true }
                Button { objectName: "closeCancel"; text: "Cancel"; onClicked: window.cancelClose() }
                Button { objectName: "closeSave"; text: "Save"; highlighted: true; onClicked: window.saveBeforeClosing() }
            }
        }
    }

    FileDialog {
        id: openDialog; title: "Open Mindarchy document"; nameFilters: ["Mindmap documents (*.omm *.json)", "Open Mindmap (*.omm)", "Legacy JSON (*.json)"]
        onAccepted: { if (!window.commitEditor("")) return; controller.requestOpenDocument(window.localPath(selectedFile)); canvas.forceActiveFocus() }
    }
    FileDialog {
        id: saveDialog; title: "Save Mindarchy document"; fileMode: FileDialog.SaveFile
        nameFilters: ["Open Mindmap (*.omm)"]; defaultSuffix: "omm"
        onAccepted: window.finishSaveDialog(window.localPath(selectedFile))
        onRejected: window.finishSaveDialog("")
    }
    FileDialog {
        id: imageDialog; objectName: "exportImageDialog"; title: "Export current canvas view"; fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]; defaultSuffix: "png"
        onAccepted: { if (!window.commitEditor("")) return; canvas.exportPng(window.localPath(selectedFile)) }
    }

    ShareDialog {
        id: shareDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            objectName: "mainToolbar"
            Layout.fillWidth: true; implicitHeight: 60
            color: ShellTheme.colors["#19242d"] || "#19242d"
            MouseArea {
                objectName: "headerDragArea"
                anchors.fill: parent
                anchors.leftMargin: window.integratedMacToolbar && window.hostWindow.visibility !== Window.FullScreen ? 96 : 0
                anchors.rightMargin: window.windowsCaptionWidth
                enabled: window.integratedToolbar
                acceptedButtons: Qt.LeftButton
                onPressed: window.hostWindow.startSystemMove()
                onDoubleClicked: window.hostWindow.visibility === Window.Maximized ? window.hostWindow.showNormal() : window.hostWindow.showMaximized()
            }
            Row {
                objectName: "windowControls"
                anchors.right: parent.right
                height: parent.height
                visible: window.windowsCaptionWidth > 0
                Repeater {
                    model: 3
                    Button {
                        required property int index
                        objectName: ["minimizeWindow", "maximizeWindow", "closeWindow"][index]
                        focusPolicy: Qt.NoFocus
                        width: 46; height: 60
                        hoverEnabled: true
                        text: index === 0 ? "Minimize" : index === 1
                            ? (window.hostWindow.visibility === Window.Maximized ? "Restore" : "Maximize") : "Close"
                        background: Rectangle {
                            color: parent.down ? (parent.index === 2 ? "#b3261e" : "#455560")
                                : parent.hovered ? (parent.index === 2 ? "#c42b1c" : "#344650") : "transparent"
                        }
                        contentItem: Text {
                            text: parent.index === 0 ? "\ue921" : parent.index === 1
                                ? (window.hostWindow.visibility === Window.Maximized ? "\ue923" : "\ue922") : "\ue8bb"
                            font.family: "Segoe MDL2 Assets"; font.pixelSize: 11
                            color: parent.index === 2 && parent.hovered ? "white" : window.ink
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: text
                        onClicked: {
                            if (index === 0) window.hostWindow.showMinimized()
                            else if (index === 1) {
                                if (window.hostWindow.visibility === Window.Maximized) window.hostWindow.showNormal()
                                else window.hostWindow.showMaximized()
                            } else window.hostWindow.close()
                        }
                    }
                }
            }
                        ToolbarButton {
                            id: applicationMenuButton; objectName: "applicationMenuButton"
                            anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
                            ToolTip.visible: hovered && !applicationMenu.visible
                            visible: Qt.platform.os === "linux"; iconName: "menu"; text: "Application menu"
                            checked: applicationMenu.visible
                            onClicked: applicationMenu.visible ? applicationMenu.close() : applicationMenu.open()
                            Menu {
                                id: applicationMenu; objectName: "applicationMenu"
                                y: applicationMenuButton.height+4; x: applicationMenuButton.width-width
                                Component.onCompleted: { if(Qt.platform.os === "linux") { addMenu(desktopMenus.fileMenu); addMenu(desktopMenus.windowMenu); addMenu(desktopMenus.helpMenu) } }
                            }
                        }
            Flickable {
                id: toolbarViewport; objectName: "toolbarViewport"
                anchors.fill: parent
                anchors.leftMargin: window.integratedMacToolbar && window.hostWindow.visibility !== Window.FullScreen ? 96 : 12
                anchors.rightMargin: 12 + window.windowsCaptionWidth + (applicationMenuButton.visible ? applicationMenuButton.width + 8 : 0)
                interactive: contentWidth > width
                contentWidth: toolbarRow.width; contentHeight: height; clip: true
                flickableDirection: Flickable.HorizontalFlick
                Item {
                    id: toolbarRow
                    readonly property real groupGap: 12
                    width: Math.max(toolbarViewport.width, documentActions.width + editingActions.width + panelActions.width + groupGap * 2)
                    height: parent.height
                    RowLayout {
                        id: documentActions; objectName: "documentActions"
                        anchors.left: parent.left; height: parent.height
                        width: implicitWidth; spacing: window.width < 800 ? 4 : 6
                        Item {
                            id: documentTitleArea
                            visible: window.width >= 1100
                            readonly property bool canReveal: { controller.documentName; return window.integratedMacToolbar && controller.documentPath().length > 0 }
                            implicitWidth: Math.min(180, documentNameLabel.implicitWidth) + 20
                            implicitHeight: documentNameLabel.implicitHeight + documentEditedLabel.implicitHeight + 2
                            property real editedProgress: window.documentEdited ? 1 : 0
                            Behavior on editedProgress { NumberAnimation { duration: 240; easing.type: Easing.InOutCubic } }
                            Layout.leftMargin: 6; Layout.rightMargin: 12
                            Image {
                                source: "qrc:/qml/icons/folder-open.svg"
                                width: 16; height: 16
                                y: documentNameLabel.y + (documentNameLabel.height-height)/2
                                opacity: titleMouse.containsMouse && documentTitleArea.canReveal ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 140 } }
                            }
                            Item {
                                id: documentTitleColumn
                                width: parent.width; height: parent.height
                            Label { id: documentNameLabel; objectName: "documentNameLabel"; text: controller.documentName; textFormat: Text.PlainText; width: Math.min(180, implicitWidth); elide: Text.ElideRight; font.pixelSize: 16; font.bold: true; color: window.ink
                                x: titleMouse.containsMouse && documentTitleArea.canReveal ? 20 : 0
                                Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                                y: (parent.height-height)/2 - documentTitleArea.editedProgress*(documentEditedLabel.height+2)/2
                            }
                            Label {
                                id: documentEditedLabel; objectName: "documentEditedLabel"
                                y: documentNameLabel.y + documentNameLabel.height + 2
                                opacity: documentTitleArea.editedProgress; visible: opacity > 0
                                text: "Edited"; color: window.muted; font.pixelSize: 10
                            }
                            }
                            MouseArea {
                                id: titleMouse; objectName: "documentTitleMouse"
                                anchors.fill: parent; hoverEnabled: true
                                enabled: documentTitleArea.canReveal
                                acceptedButtons: Qt.RightButton
                                onClicked: function(mouse) {
                                    var point = mapToItem(null, mouse.x, mouse.y)
                                    controller.nativeFolderMenuRequested(point.x, point.y)
                                }
                            }
                        }
                        ToolbarButton {
                            id: fileButton; objectName: "fileMenuButton"
                            iconName: "files"; text: "File  ▾"
                            Accessible.name: "File menu"
                            ToolTip.text: "New, open, save and export"
                            display: AbstractButton.TextBesideIcon
                            implicitWidth: 92; implicitHeight: window.width < 800 ? 32 : 36; font.pixelSize: 13
                            checked: fileMenu.visible
                            onClicked: fileMenu.visible ? fileMenu.close() : fileMenu.open()
                            Keys.onDownPressed: fileMenu.open()
                            Keys.onReturnPressed: fileMenu.open()
                            Menu {
                                id: fileMenu; objectName: "fileActionsMenu"
                                y: fileButton.height + 8; x: 0; width: 240; padding: 6
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                onOpened: { currentIndex = 0; fileNew.forceActiveFocus() }
                                background: Rectangle {
                                    radius: 10
                                    color: ShellTheme.colors["#172129"] || "#172129"
                                    border.color: ShellTheme.colors["#34434c"] || "#34434c"
                                }
                                MenuItem {
                                    id: fileNew; objectName: "newDocumentButton"
                                    text: "New map"; icon.source: "qrc:/qml/icons/file-plus-2.svg"; icon.color: window.ink
                                    onTriggered: { fileMenu.close(); if (window.commitEditor("")) controller.newDocumentRequested() }
                                }
                                MenuItem {
                                    objectName: "openDocumentButton"
                                    text: "Open map…"; icon.source: "qrc:/qml/icons/folder-open.svg"; icon.color: window.ink
                                    onTriggered: { fileMenu.close(); openDialog.open() }
                                }
                                MenuSeparator {}
                                MenuItem {
                                    objectName: "saveDocumentButton"
                                    text: "Save map"; icon.source: "qrc:/qml/icons/save.svg"; icon.color: window.ink
                                    onTriggered: { fileMenu.close(); window.saveDocument(false) }
                                }
                                MenuItem {
                                    objectName: "exportDocumentButton"
                                    text: "Export as PNG…"; icon.source: "qrc:/qml/icons/image-down.svg"; icon.color: window.ink
                                    onTriggered: { fileMenu.close(); if (window.commitEditor("")) imageDialog.open() }
                                }
                            }
                        }
                        ToolbarButton {
                            id: addMenuButton; objectName: "addMenuButton"
                            iconName: "plus"; text: "Add  ▾"
                            Accessible.name: "Add menu"
                            ToolTip.text: "New child, template or sibling nodes"
                            display: AbstractButton.TextBesideIcon
                            implicitWidth: 92; implicitHeight: window.width < 800 ? 32 : 36; font.pixelSize: 13
                            checked: addMenu.visible
                            onClicked: addMenu.visible ? addMenu.close() : addMenu.open()
                            Keys.onDownPressed: addMenu.open()
                            Keys.onReturnPressed: addMenu.open()
                            Menu {
                                id: addMenu; objectName: "addActionsMenu"
                                y: addMenuButton.height + 8; x: 0; width: 240; padding: 6
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                onOpened: { currentIndex = 0; addChildItem.forceActiveFocus() }
                                background: Rectangle {
                                    radius: 10
                                    color: ShellTheme.colors["#172129"] || "#172129"
                                    border.color: ShellTheme.colors["#34434c"] || "#34434c"
                                }
                                MenuItem {
                                    id: addChildItem; objectName: "addChildButton"
                                    text: "Add child"; icon.source: "qrc:/qml/icons/corner-down-right.svg"; icon.color: window.ink
                                    onTriggered: { addMenu.close(); if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addChild() }
                                }
                                MenuItem {
                                    id: nodeTemplatesItem; objectName: "nodeTemplatesButton"
                                    text: "Add node template"; icon.source: "qrc:/qml/icons/calendar-week.svg"; icon.color: window.ink
                                    enabled: controller.selection.length === 1
                                    onTriggered: { addMenu.close(); if (!window.commitEditor("")) return;
                                        nodeTemplateDialog.open() }
                                }
                                MenuItem {
                                    objectName: "addSiblingButton"
                                    text: "Add sibling"; icon.source: "qrc:/qml/icons/list-plus.svg"; icon.color: window.ink
                                    onTriggered: { addMenu.close(); if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addSibling() }
                                }
                            }
                            NodeTemplateDialog {
                                id: nodeTemplateDialog
                                controller: window.controller
                            }
                        }
                        ToolbarButton {
                            id: shareButton; objectName: "shareButton"
                            iconName: "share"; text: "Share map"
                            checked: shareDialog.visible
                            onClicked: { if (window.commitEditor("")) shareDialog.open() }
                        }
                    }
                    RowLayout {
                        id: editingActions; objectName: "editingActions"
                        // Center in the window, including the macOS traffic-light
                        // inset, then clamp between the fixed outer groups.
                        x: Math.max(documentActions.width + toolbarRow.groupGap,
                            Math.min((window.width - width) / 2 - toolbarViewport.x,
                                panelActions.x - width - toolbarRow.groupGap))
                        height: parent.height; width: implicitWidth; spacing: window.width < 800 ? 4 : 6
                        ToolbarButton { iconName: "undo-2"; text: "Undo"; enabled: controller.canUndo; onClicked: { if (!window.commitEditor("")) return; controller.undo() } }
                        ToolbarButton { iconName: "redo-2"; text: "Redo"; enabled: controller.canRedo; onClicked: { if (!window.commitEditor("")) return; controller.redo() } }
                        Rectangle { implicitWidth: 1; implicitHeight: 24; color: (ShellTheme.colors["#34434c"] || "#34434c") }
                        ToolbarButton { iconName: controller.selectedFolded ? "unfold-vertical" : "fold-vertical"; text: controller.selectedFolded ? "Expand branch" : "Fold branch"; onClicked: { if (!window.commitEditor("")) return; controller.toggleFold() } }
                    }
                    RowLayout {
                        id: panelActions; objectName: "panelActions"
                        anchors.right: parent.right; height: parent.height
                        width: implicitWidth; spacing: window.width < 800 ? 4 : 6
                        ToolbarButton { objectName: "searchButton"; iconName: "search"; text: "Search mind map"; checked: window.searchOpen; onClicked: window.openSearch() }
                        RowLayout {
                            visible: window.searchOpen; spacing: 4
                            TextField {
                                id: searchField; objectName: "mindmapSearchField"
                                Layout.preferredWidth: 180; placeholderText: "Search mind map…"; selectByMouse: true
                                Accessible.name: "Search mind map"
                                onTextChanged: { window.searchIndex=-1; canvas.clearSearchHighlight(); searchFlash.stop(); searchHighlight.opacity=0; searchDelay.restart() }
                                onAccepted: window.nextSearchResult()
                                Keys.onEscapePressed: { window.searchOpen=false; searchDelay.stop(); canvas.clearSearchHighlight(); searchFlash.stop(); searchHighlight.opacity=0; canvas.forceActiveFocus() }
                            }
                            Label { objectName: "searchResultCount"; text: searchField.text.trim().length ? (window.searchResults.length ? (window.searchIndex+1) + " / " + window.searchResults.length : "No matches") : ""; color: window.muted }
                            ToolButton { text: "×"; Accessible.name: "Close search"; focusPolicy: Qt.NoFocus
                                background: Rectangle { radius: 6; color: parent.down ? (ShellTheme.colors["#35505a"] || "#35505a") : parent.hovered ? (ShellTheme.colors["#2a3d48"] || "#2a3d48") : "transparent" }
 onClicked: { window.searchOpen=false; searchDelay.stop(); canvas.clearSearchHighlight(); searchFlash.stop(); searchHighlight.opacity=0; canvas.forceActiveFocus() } }
                        }
                        RowLayout {
                            id: zoomControls; objectName: "zoomControls"; spacing: 0
                            ToolbarButton {
                                id: zoomButton; objectName: "zoomPercentage"
                                iconName: ""; text: "Zoom options"; Layout.preferredWidth: 76
                                checked: zoomMenu.visible
                                ToolTip.visible: hovered && !zoomMenu.visible
                                onClicked: zoomMenu.visible ? zoomMenu.close() : zoomMenu.open()
                                contentItem: RowLayout {
                                    spacing: 6
                                    Text {
                                        Layout.fillWidth: true
                                        text: (canvas.zoom * 100).toFixed(canvas.zoom < .1 ? 2 : 0) + "%"
                                        color: window.ink; font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                    Text { text: "▾"; color: window.ink; font.pixelSize: 12 }
                                }
                                Popup {
                                    id: zoomMenu; objectName: "zoomMenu"
                                    focus: true
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                    onOpened: zoomInItem.forceActiveFocus()
                                    y: zoomButton.height + 4; x: zoomButton.width - width
                                    width: 200; padding: 4
                                    background: Rectangle {
                                        radius: 8; color: window.hostWindow.palette.window
                                        border.color: (ShellTheme.colors["#34434c"] || "#34434c")
                                    }
                                    contentItem: Column {
                                        ZoomMenuItem { id: zoomInItem; objectName: "zoomInAction"; text: "Zoom In"; icon.source: "qrc:/qml/icons/zoom-in.svg"; onClicked: canvas.zoomIn(); KeyNavigation.down: zoomOutItem; KeyNavigation.up: zoomFitItem }
                                        ZoomMenuItem { id: zoomOutItem; objectName: "zoomOutAction"; text: "Zoom Out"; icon.source: "qrc:/qml/icons/zoom-out.svg"; onClicked: canvas.zoomOut(); KeyNavigation.down: zoomActualItem; KeyNavigation.up: zoomInItem }
                                        MenuSeparator { width: parent.width }
                                        ZoomMenuItem { id: zoomActualItem; objectName: "zoomActualSizeAction"; text: "Actual Size (100%)"; icon.source: "qrc:/qml/icons/scan.svg"; onClicked: { canvas.resetZoom(); zoomMenu.close() } KeyNavigation.down: zoomFitItem; KeyNavigation.up: zoomOutItem }
                                        ZoomMenuItem { id: zoomFitItem; objectName: "zoomFitAction"; text: "Fit Map"; icon.source: "qrc:/qml/icons/maximize.svg"; onClicked: { canvas.fit(); zoomMenu.close() } KeyNavigation.down: zoomInItem; KeyNavigation.up: zoomActualItem }
                                    }
                                }
                            }
                        }
                        Rectangle { implicitWidth: 1; implicitHeight: 24; color: (ShellTheme.colors["#34434c"] || "#34434c"); Layout.leftMargin: 4; Layout.rightMargin: 4 }
                        ToolbarButton { iconName: "panel-left"; text: "Toggle outline"; checkable: true; checked: window.outlineVisible; onClicked: window.hostWindow.outlineVisible = !window.hostWindow.outlineVisible }
                        ToolbarButton { iconName: "panel-right"; text: "Toggle inspector"; checkable: true; checked: window.inspectorVisible; onClicked: window.hostWindow.inspectorVisible = !window.hostWindow.inspectorVisible }

                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                objectName: "outlineSidebar"; visible: window.outlineVisible; Layout.minimumWidth: 224; Layout.maximumWidth: 224; Layout.preferredWidth: 224; Layout.fillHeight: true; color: (ShellTheme.colors["#17232c"] || "#17232c")
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
                            color: controller.selectedId === modelData.id ? (ShellTheme.colors["#29463f"] || "#29463f") : rowMouse.containsMouse ? (ShellTheme.colors["#22323d"] || "#22323d") : "transparent"
                            Label { anchors.left: parent.left; anchors.leftMargin: 8 + Math.min(modelData.depth, 7) * 10; anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: (modelData.folded ? "▸ " : "· ") + modelData.text; textFormat: Text.PlainText; elide: Text.ElideRight; color: controller.selectedId === modelData.id ? window.accent : (ShellTheme.colors["#a5b5bf"] || "#a5b5bf"); font.pixelSize: 12 }
                            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; onClicked: function(mouse) { if (!window.commitEditor("")) return; controller.select(modelData.id, !!(mouse.modifiers & Qt.ShiftModifier)); canvas.forceActiveFocus() }
                onDoubleClicked: canvas.beginEdit(modelData.id) }
                        }
                    }
                    Rule {}
                    Label { text: "Tab to grow a branch.\nReturn to add a sibling."; color: window.muted; font.pixelSize: 11; lineHeight: 1.5 }
                }
            }
            Rectangle { visible: window.outlineVisible; Layout.preferredWidth: 1; Layout.fillHeight: true; color: (ShellTheme.colors["#2a3943"] || "#2a3943") }
            Item {
                id: centerColumn; objectName: "centerWorkspace"
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 320
                DocumentTabStrip {
                    id: documentStrip
                    anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                    model: window.documentTabs; activeDocumentId: window.hostWindow.documentTabId
                    onActivateRequested: function(id) { controller.tabActionRequested("activate", id) }
                    onCloseRequested: function(id) { controller.tabActionRequested("close", id) }
                    onNewRequested: controller.tabActionRequested("new", 0)
                    onMoveRequested: function(id, targetIndex) { window.hostWindow.tabMoveRequested(id, targetIndex) }
                    onDetachRequested: function(id) { controller.tabActionRequested("detach", id) }
                    onDocumentFocusRequested: window.focusDocument()
                }
                Rectangle {
                    objectName: "documentErrorBanner"
                    anchors.top: documentStrip.bottom; anchors.left: parent.left; anchors.right: parent.right
                    visible: controller.error.length > 0; z: 30
                    height: errorLabel.implicitHeight + 20
                    color: ShellTheme.colors["#563832"] || "#563832"
                    Label { id: errorLabel; anchors.fill: parent; anchors.margins: 10; text: controller.error; color: ShellTheme.colors["#ffd3c7"] || "#ffd3c7"; wrapMode: Text.Wrap }
                }
                Connections { target: controller; function onClipboardMessage(message) { window.exportStatus=message; exportTimer.restart() } }
                Rectangle {
                    objectName: "focusBreadcrumbBar"
                    anchors.top: documentStrip.bottom; anchors.left: parent.left; anchors.right: parent.right
                    height: 36; z: 20; visible: canvas.focusActive
                    color: window.controller.canvasColor
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
                        Flickable {
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                            contentWidth: crumbs.width; contentHeight: height
                            Row {
                                id: crumbs; height: parent.height; spacing: 4
                                Repeater {
                                    model: canvas.focusBreadcrumb
                                    Button {
                                        required property var modelData
                                        text: modelData.text; height: 32
                                        width: Math.min(220,implicitWidth)
                                        onClicked: { if(window.commitEditor("")) canvas.focusBranch(modelData.id); canvas.forceActiveFocus() }
                                    }
                                }
                            }
                        }
                        Button { text: "Exit Focus · Esc"; onClicked: { if(window.commitEditor("")) canvas.exitFocus();canvas.forceActiveFocus() } }
                    }
                }
                MindCanvas {
                    id: canvas; objectName: "mindCanvas"; anchors.fill: parent; anchors.topMargin: documentStrip.height; engine: window.controller; focus: true
                    NodeImageTools { id: nodeImageTools; anchors.fill: parent; z: 9; canvas: parent; controller: window.controller; hostWindow: window.hostWindow }
                    onImageMenuRequested: function(id,x,y) { nodeImageTools.showMenu(id,x,y) }
                    onImagePreviewRequested: function(id) { nodeImageTools.preview(id) }
                    onCommitRequested: window.commitEditor("")
                    onSearchResultFocused: searchFlash.restart()
                    Rectangle {
                        id: searchHighlight; objectName: "searchResultFlash"
                        x: canvas.searchResultRect.x - 6; y: canvas.searchResultRect.y - 6
                        width: canvas.searchResultRect.width + 12; height: canvas.searchResultRect.height + 12
                        radius: 10; color: "transparent"; border.width: 4
                        border.color: controller.canvasColor.hslLightness > 0.5 ? "#6750b8" : "#ffe08a"
                        opacity: 0; z: 10
                        SequentialAnimation on opacity {
                            id: searchFlash; running: false
                            NumberAnimation { from: 0; to: 1; duration: 90 }
                            NumberAnimation { to: 0; duration: 650 }
                        }
                    }
                    onDateEditRequested: function(id, date, text) {
                        if (window.commitEditor("")) dateDialog.openEntry(id,date,text)
                    }
                    ToolTip {
                        objectName: "dateEntryTooltip"
                        text: canvas.dateHoverText
                        visible: canvas.dateHoverText.length>0 && !dateDialog.opened && !canvas.dragging
                        delay: 0
                        width: Math.min(implicitWidth, 300, canvas.width)
                        x: Math.max(0,Math.min(canvas.width-width,canvas.dateHoverPosition.x+12))
                        y: Math.max(0,Math.min(canvas.height-height,canvas.dateHoverPosition.y+16))
                    }
                    onEditRequested: function(id, text) { editor.text = text; editor.initialText = editor.text; editor.forceActiveFocus(); editor.selectAll() }
                    onReplaceEditingText: function(text) { editor.text = text; editor.cursorPosition = editor.length }
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
                        TextEdit {
                            id: editor; objectName: "titleEditor"
                            property string initialText: ""
                            property bool normalizingSize: false
                            x: inlineEditor.textInset
                            y: Math.max(8, (inlineEditor.height - contentHeight) / 2)
                            width: Math.max(1, inlineEditor.width - inlineEditor.textInset - 15)
                            height: Math.max(22, inlineEditor.height - y)
                            clip: true; textMargin: 0
                            textFormat: TextEdit.RichText; wrapMode: TextEdit.Wrap
                            font.family: controller.textFamily; font.pixelSize: inlineEditor.nodeAppearance.fontSize || 20
                            color: inlineEditor.nodeAppearance.text || "#f1fff9"; selectionColor: "#438b78"
                            onTextChanged: {
                                if (canvas.editing && !normalizingSize) {
                                    normalizingSize = true
                                    canvas.normalizeEditorSize(editor)
                                    normalizingSize = false
                                    canvas.updateEditingText(text)
                                }
                            }
                            Text {
                                visible: editor.length===0 && !editor.inputMethodComposing
                                text: controller.selectedEntryPrompt; color: "#718896"; font: editor.font
                                width: parent.width; elide: Text.ElideRight
                            }
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
            }
            Rectangle { visible: window.inspectorVisible; Layout.preferredWidth: 1; Layout.fillHeight: true; color: (ShellTheme.colors["#2a3943"] || "#2a3943") }
            Rectangle {
                objectName: "inspectorSidebar"; visible: window.inspectorVisible; Layout.minimumWidth: 274; Layout.maximumWidth: 274; Layout.preferredWidth: 274; Layout.fillHeight: true; color: (ShellTheme.colors["#19252e"] || "#19252e")
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 15
                    Caption { text: "INSPECTOR" }
                    TabBar {
                        id: inspectorTabs; objectName: "inspectorTabs"; Layout.fillWidth: true
                        TabButton { text: "Map"; icon.source: "qrc:/qml/icons/inspector-map.svg"; icon.width: 16; icon.height: 16; icon.color: window.ink; spacing: 4; leftPadding: 3; rightPadding: 3 }
                        TabButton { text: "Node"; icon.source: "qrc:/qml/icons/inspector-node.svg"; icon.width: 16; icon.height: 16; icon.color: window.ink; spacing: 4; leftPadding: 3; rightPadding: 3 }
                        TabButton { text: "Theme"; objectName: "themesTab"; icon.source: "qrc:/qml/icons/inspector-themes.svg"; icon.width: 16; icon.height: 16; icon.color: window.ink; spacing: 4; leftPadding: 3; rightPadding: 3 }
                    }
                    ScrollView {
                        objectName: "inspectorScroll"
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        contentWidth: availableWidth
                        ColumnLayout {
                            width: parent.width; spacing: 18
                            ColumnLayout {
                                visible: inspectorTabs.currentIndex === 0; Layout.fillWidth: true; spacing: 12
                                Caption { text: "DIRECTION" }
                                MapOptionBar { selectedValue: controller.layout; options: [{"value": "Horizontal", "label": "Horizontal \u2014 branches expand sideways", "path": "M3 10h5v4H3z M16 3h5v4h-5z M16 17h5v4h-5z M8 12h4 M12 5v14 M12 5h4 M12 19h4"}, {"value": "Vertical", "label": "Vertical \u2014 branches expand downward", "path": "M10 3h4v5h-4z M3 16h4v5H3z M17 16h4v5h-4z M12 8v4 M5 12h14 M5 12v4 M19 12v4"}, {"value": "Compact", "label": "Compact \u2014 tightly arranged branches", "path": "M3 10h5v4H3z M16 3h5v4h-5z M16 10h5v4h-5z M16 17h5v4h-5z M8 12h8 M12 5v14 M12 5h4 M12 19h4"}]
                                    onChosen: function(value) { if (!window.commitEditor("")) return; controller.layout = value }
                                }
                                Caption { text: "SPACING" }
                                MapOptionBar { enabled: !controller.manual && controller.layout !== "Compact"; selectedValue: controller.spacing; options: [{"value": "Narrow", "label": "Narrow spacing", "path": "M3 5h18 M3 19h18 M8 10h8v4H8z M12 6v3 M10 7l2 2 2-2 M12 18v-3 M10 17l2-2 2 2"}, {"value": "Standard", "label": "Standard spacing", "path": "M3 3h18 M3 21h18 M8 10h8v4H8z M12 5v3 M10 6l2 2 2-2 M12 19v-3 M10 18l2-2 2 2"}, {"value": "Wide", "label": "Wide spacing", "path": "M3 2h18 M3 22h18 M8 10h8v4H8z M12 8V4 M10 6l2-2 2 2 M12 16v4 M10 18l2 2 2-2"}]
                                    onChosen: function(value) { if (!window.commitEditor("")) return; controller.spacing = value }
                                }
                                Caption { text: "CONNECTIONS" }
                                ComboBox {
                                    id: branchStylePicker
                                    objectName: "branchStylePicker"
                                    Layout.fillWidth: true
                                    model: controller.branchStyles
                                    currentIndex: controller.branchStyles.indexOf(controller.branchStyle)
                                    Accessible.name: "Branch style"
                                    onActivated: function(index) {
                                        if (window.commitEditor("")) controller.branchStyle = controller.branchStyles[index]
                                        currentIndex = Qt.binding(function() { return controller.branchStyles.indexOf(controller.branchStyle) })
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                                    text: "Artistic branches adapt to light and dark map backgrounds."
                                    color: window.muted; font.pixelSize: 11
                                }
                                Caption { text: "PLACEMENT" }
                                MapOptionBar {
                                    enabled: controller.layout !== "Compact"
                                    selectedValue: controller.manual ? "Manual" : "Automatic"
                                    options: [
                                        { value: "Automatic", label: "Automatic placement — arrange branches automatically", path: "M3 3h6v6H3z M15 3h6v6h-6z M3 15h6v6H3z M15 15h6v6h-6z" },
                                        { value: "Manual", label: "Manual placement — drag nodes to position them freely", path: "M12 3v18 M3 12h18 M9 6l3-3 3 3 M9 18l3 3 3-3 M6 9l-3 3 3 3 M18 9l3 3-3 3" }
                                    ]
                                    onChosen: function(value) { if (!window.commitEditor("")) return; controller.manual = value === "Manual" }
                                }
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
                                enabled: controller.selection.length > 0
                                opacity: enabled ? 1 : 0.65
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: typeGroup.height
                                    color: (ShellTheme.colors["#122029"] || "#122029"); radius: 8; border.color: (ShellTheme.colors["#2a3943"] || "#2a3943")
                                    Column {
                                        id: typeGroup; width: parent.width; spacing: 0
                                        MapOptionBar {
                                            width: parent.width; optionPrefix: "node-type-"
                                            enabled: controller.selection.length === 1
                                            selectedValue: controller.selectedKind === "date" ? "Date" : controller.selectedTask ? "Task" : "Text"
                                            options: [
                                                {value: "Text", label: "Text", path: "M4 5h16 M12 5v14 M8 19h8"},
                                                {value: "Task", label: "Task", path: "M9 11l3 3L22 4 M21 12v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11"},
                                                {value: "Date", label: "Date", path: "M8 2v4 M16 2v4 M3 10h18 M5 4h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2 M8 14h.01 M12 14h.01 M16 14h.01 M8 18h.01 M12 18h.01"}
                                            ]
                                            onChosen: function(value) {
                                                if(!window.commitEditor("")) return
                                                controller.setNodeKind(controller.selectedId,value.toLowerCase())
                                                canvas.revealNode(controller.selectedId)
                                            }
                                        }
                                        Item {
                                            objectName: "nodeTypeOptions"
                                            width: parent.width
                                            visible: controller.selectedKind === "date" || controller.selectedTask
                                            height: visible ? typeOptions.implicitHeight + 24 : 0
                                            enabled: controller.selection.length === 1
                                            Rectangle { x: 12; width: parent.width-24; height: 1; color: (ShellTheme.colors["#2a3943"] || "#2a3943") }
                                            ColumnLayout {
                                                id: typeOptions; x: 12; y: 12; width: parent.width-24; spacing: 8
                                                Caption { visible: controller.selectedTask; text: "TASK" }
                                                CheckBox {
                                                    objectName: "taskCompleted"; visible: controller.selectedTask && controller.selectedTaskChildren===0
                                                    text: "Completed"; checked: controller.selectedChecked
                                                    onClicked: { if(window.commitEditor("")) controller.toggleChecked() }
                                                }
                                                Label {
                                                    visible: controller.selectedTask && controller.selectedTaskChildren>0
                                                    text: controller.selectedCompletedTasks + " / " + controller.selectedTaskChildren + " tasks completed"
                                                    color: window.ink
                                                }
                                                DateNodePanel {
                                                    visible: controller.selectedKind === "date"; Layout.fillWidth: true
                                                    controller: window.controller; canvas: canvas; commitEditor: window.commitEditor
                                                }
                                            }
                                        }
                                    }
                                }
                                ColumnLayout {
                                    objectName: "imagePlacementSection"
                                    visible: controller.selection.length === 1 && controller.selectedHasImage
                                    Layout.fillWidth: true; spacing: 8
                                    Caption { text: "IMAGE PLACEMENT" }
                                    MapOptionBar {
                                        Layout.fillWidth: true; optionPrefix: "image-placement-"
                                        selectedValue: controller.selectedImagePlacement
                                        options: [
                                            {value:"left",label:"Image on left",path:"M3 5h7v14H3z M14 7h7 M14 12h7 M14 17h7"},
                                            {value:"right",label:"Image on right",path:"M14 5h7v14h-7z M3 7h7 M3 12h7 M3 17h7"},
                                            {value:"top",label:"Image on top",path:"M5 3h14v7H5z M5 14h14 M5 19h14"},
                                            {value:"bottom",label:"Image on bottom",path:"M5 14h14v7H5z M5 4h14 M5 9h14"}
                                        ]
                                        onChosen: function(value) { if(window.commitEditor("")) controller.setImagePlacement(controller.selectedId,value) }
                                    }
                                }
                                MeetingPanel { Layout.fillWidth: true; controller: window.controller }
                                ResourcePanel { id: resourcePanel; Layout.fillWidth: true; controller: window.controller; commitEditor: window.commitEditor }
                                Rule {}
                                NodeStylePanel { calendarNode: controller.selectedKind === "date"; Layout.fillWidth: true; controller: window.controller; commitEditor: window.commitEditor }
                                SmallButton { visible: controller.selectedKind !== "date"; text: "Edit title"; Layout.fillWidth: true; onClicked: { if (!window.commitEditor("")) return; canvas.editSelected() } }
                                Rule {}
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
                }
            }
        }
    }
    // Export feedback remains available after removing the statistics footer.
    Rectangle {
        visible: window.exportStatus.length > 0
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 80
        width: exportMessage.implicitWidth + 28; height: 38; radius: 8; color: (ShellTheme.colors["#29463f"] || "#29463f")
        Label { id: exportMessage; anchors.centerIn: parent; text: window.exportStatus; color: window.ink }
    }
    Timer { id: exportTimer; interval: 5000; onTriggered: window.exportStatus = "" }
    Component.onCompleted: Qt.callLater(function() { canvas.initializeView(); if (window.visible) canvas.forceActiveFocus() })
}
