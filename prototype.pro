QT += core gui qml quick quickcontrols2
CONFIG += c++20
TARGET = mindmap-lab
SOURCES += src/main.cpp src/engine.cpp src/theme.cpp src/canvas.cpp
HEADERS += src/engine.h src/theme.h src/canvas.h
RESOURCES += resources.qrc

macx {
    ICON = assets/icons/mindmap-blue.icns
    QMAKE_MACOSX_BUNDLE_GUI_IDENTIFIER = blue.mindmap.lab
}
win32: RC_ICONS = assets/icons/mindmap-blue.ico
unix:!macx {
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    desktop.files = packaging/blue.mindmap.lab.desktop
    desktop.path = $$PREFIX/share/applications
    INSTALLS += target desktop
    icon.files = assets/icons/blue.mindmap.lab.png
    icon.path = $$PREFIX/share/icons/hicolor/512x512/apps
    INSTALLS += icon
}
