import QtQuick
import QtQuick.Controls

Item {
    id: menus
    required property var host
    required property var controller
    required property var shortcutsWindow
    property alias fileMenu: fileMenu
    property alias windowMenu: windowMenu
    property alias helpMenu: helpMenu
    readonly property bool popupOpen: recentMenu.opened || fileMenu.opened || windowMenu.opened || helpMenu.opened
    signal commandChosen()
    signal menusClosed()
    function closeMenus() { recentMenu.close(); fileMenu.close(); windowMenu.close(); helpMenu.close() }
    function cycle(direction) { controller.cycleApplicationWindow(direction) }
    Menu {
        id: fileMenu; objectName: "desktopFileMenu"; title: qsTr("&File")
        onClosed: menus.menusClosed()
        MenuItem { objectName: "desktopNewAction"; text: qsTr("&New"); onTriggered: { menus.commandChosen(); controller.newDocumentRequested() } }
        MenuItem { text: qsTr("&Open…"); onTriggered: { menus.commandChosen(); host.openDocumentMenu() } }
        Menu {
            id: recentMenu; objectName: "desktopRecentMenu"; title: qsTr("Open &Recent")
            property var documents: []
            onAboutToShow: documents=controller.recentDocuments()
            MenuItem { text: qsTr("No Recent Files"); enabled: false; visible: recentMenu.documents.length===0 }
            Instantiator {
                model: recentMenu.documents
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.name.replace(/&/g,"&&") + " — " + modelData.path.replace(/&/g,"&&")
                    enabled: modelData.available
                    onTriggered: { menus.commandChosen(); controller.requestOpenDocument(modelData.path) }
                }
                onObjectAdded: function(index,object) { recentMenu.insertItem(index,object) }
                onObjectRemoved: function(index,object) { recentMenu.removeItem(object) }
            }
            MenuSeparator {}
            MenuItem { objectName: "clearRecentAction"; text: qsTr("Clear Menu"); enabled: recentMenu.documents.length>0; onTriggered: { controller.clearRecentDocuments(); recentMenu.documents=[] } }
        }
        MenuItem { text: qsTr("&Save"); onTriggered: { menus.commandChosen(); host.saveDocument(false) } }
        MenuSeparator {}
        MenuItem { text: qsTr("&Close window"); onTriggered: { menus.commandChosen(); host.requestClose(true,false) } }
        MenuItem { text: qsTr("E&xit Mindarchy"); onTriggered: { menus.commandChosen(); controller.quitRequested() } }
    }
    Menu {
        id: windowMenu; objectName: "desktopWindowMenu"; title: qsTr("&Window")
        property var openWindows: []
        onAboutToShow: openWindows=controller.applicationWindows()
        onClosed: menus.menusClosed()
        MenuItem { text: qsTr("Minimize"); onTriggered: { menus.commandChosen(); host.showMinimized() } }
        MenuItem { text: qsTr("Zoom"); onTriggered: { menus.commandChosen(); host.visibility===Window.Maximized ? host.showNormal() : host.showMaximized() } }
        MenuItem {
            text: qsTr("Center"); enabled: Qt.platform.os !== "linux"
            onTriggered: { menus.commandChosen(); host.x=host.screen.virtualX+(host.screen.width-host.width)/2; host.y=host.screen.virtualY+(host.screen.height-host.height)/2 }
        }
        MenuItem { text: host.visibility===Window.FullScreen ? qsTr("Exit Full Screen") : qsTr("Enter Full Screen"); onTriggered: { menus.commandChosen(); host.visibility===Window.FullScreen ? host.showNormal() : host.showFullScreen() } }
        MenuSeparator {}
        MenuItem { text: qsTr("Next Window"); enabled: windowMenu.openWindows.length>1; onTriggered: { menus.commandChosen(); menus.cycle(1) } }
        MenuItem { text: qsTr("Previous Window"); enabled: windowMenu.openWindows.length>1; onTriggered: { menus.commandChosen(); menus.cycle(-1) } }
        MenuSeparator {}
        MenuItem { text: qsTr("Bring All to Front"); enabled: windowMenu.openWindows.length>0; onTriggered: { menus.commandChosen(); for(let entry of windowMenu.openWindows) controller.activateApplicationWindow(entry.pid) } }
        MenuSeparator { visible: windowMenu.openWindows.length>0 }
        Instantiator {
            model: windowMenu.openWindows
            delegate: MenuItem {
                required property var modelData
                text: modelData.title; checkable: true; checked: modelData.current || false
                onTriggered: { menus.commandChosen(); controller.activateApplicationWindow(modelData.pid) }
            }
            onObjectAdded: function(index,object) { windowMenu.insertItem(10+index,object) }
            onObjectRemoved: function(index,object) { windowMenu.removeItem(object) }
        }
    }
    Menu {
        id: helpMenu; objectName: "desktopHelpMenu"; title: qsTr("&Help")
        onClosed: menus.menusClosed()
        MenuItem {
            objectName: "desktopKeyboardShortcutsAction"; text: qsTr("Keyboard &Shortcuts")
            onTriggered: { menus.commandChosen(); shortcutsWindow.show(); shortcutsWindow.raise(); shortcutsWindow.requestActivate() }
        }
    }
}
