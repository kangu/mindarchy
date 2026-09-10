QT += core gui qml quick quickcontrols2
CONFIG += c++20
TARGET = mindarchy
SOURCES += src/main.cpp src/windowplacement.cpp src/drawing.cpp src/preview.cpp src/engine.cpp src/theme.cpp src/canvas.cpp src/canvasimages.cpp src/shelltheme.cpp
HEADERS += src/windowplacement.h src/documentsession.h src/documentrecovery.h src/viewportstate.h src/shelltheme.h src/engine.h src/theme.h src/canvas.h
RESOURCES += resources.qrc

macx {
    QT += network
    SOURCES += src/macapplication.cpp
    HEADERS += src/macapplication.h
    OBJECTIVE_SOURCES += src/macwindow.mm src/macplacement.mm
    LIBS += -framework AppKit -framework UniformTypeIdentifiers
    ICON = assets/icons/mindarchy.icns
    QMAKE_MACOSX_BUNDLE_GUI_IDENTIFIER = blue.mindmap.lab
}
win32: RC_ICONS = assets/icons/mindarchy.ico
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

win32 {
    SOURCES += src/windowsdialogs.cpp
    HEADERS += src/windowsdialogs.h
    LIBS += -lcomctl32 -lole32 -lshell32
}
