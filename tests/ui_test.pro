QT += core gui qml quick quickcontrols2 testlib
CONFIG += c++20 testcase console
CONFIG -= app_bundle
TARGET = ui_test
SOURCES += ui_test.cpp ../src/engine.cpp ../src/theme.cpp ../src/canvas.cpp
HEADERS += ../src/engine.h ../src/theme.h ../src/canvas.h
RESOURCES += ../resources.qrc
