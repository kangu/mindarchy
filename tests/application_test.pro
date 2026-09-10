QT += core gui qml quick quickcontrols2 testlib network
CONFIG += c++20 testcase console
CONFIG -= app_bundle
TARGET = application_test
INCLUDEPATH += ../src
SOURCES += application_test.cpp ../src/macapplication.cpp ../src/windowplacement.cpp ../src/engine.cpp ../src/theme.cpp ../src/drawing.cpp ../src/canvas.cpp ../src/canvasimages.cpp ../src/shelltheme.cpp
HEADERS += ../src/engine.h ../src/theme.h ../src/canvas.h ../src/shelltheme.h
RESOURCES += ../resources.qrc
win32 {
    SOURCES += ../src/windowsdialogs.cpp
    LIBS += -lcomctl32 -lole32 -lshell32
}
