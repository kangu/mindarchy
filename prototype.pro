QT += core gui qml quick quickcontrols2
CONFIG += c++20
TARGET = mindmap-lab
SOURCES += src/main.cpp src/windowplacement.cpp src/drawing.cpp src/preview.cpp src/engine.cpp src/theme.cpp src/canvas.cpp
HEADERS += src/windowplacement.h src/engine.h src/theme.h src/canvas.h
RESOURCES += resources.qrc

macx {
    OBJECTIVE_SOURCES += src/macwindow.mm src/macplacement.mm
    LIBS += -framework AppKit
    ICON = assets/icons/mindmap-blue.icns
    QMAKE_MACOSX_BUNDLE_GUI_IDENTIFIER = blue.mindmap.lab
}
win32: RC_ICONS = assets/icons/mindmap-blue.ico
unix:!macx {
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    desktop.files = packaging/blue.mindmap.lab.desktop
    desktop.path = $$PREFIX/share/applications
    mime.files = packaging/blue.mindmap.omm.xml
    mime.path = $$PREFIX/share/mime/packages
    thumbnailer.files = packaging/blue.mindmap.lab.thumbnailer
    thumbnailer.path = $$PREFIX/share/thumbnailers
    INSTALLS += target desktop mime thumbnailer
    icon.files = assets/icons/blue.mindmap.lab.png
    icon.path = $$PREFIX/share/icons/hicolor/512x512/apps
    INSTALLS += icon
}
