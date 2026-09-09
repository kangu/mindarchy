QT += core gui qml quick quickcontrols2 testlib
CONFIG += c++20 testcase console
CONFIG -= app_bundle
TARGET = ui_test
INCLUDEPATH += ../src
SOURCES += ui_test.cpp ../src/engine.cpp ../src/theme.cpp ../src/drawing.cpp ../src/canvas.cpp ../src/shelltheme.cpp
HEADERS += ../src/engine.h ../src/theme.h ../src/canvas.h ../src/shelltheme.h
RESOURCES += ../resources.qrc
