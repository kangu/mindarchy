QT += core gui quick testlib
CONFIG += console c++20 testcase
CONFIG -= app_bundle
TARGET = canvas_test
INCLUDEPATH += ../src
SOURCES += canvas_test.cpp ../src/engine.cpp ../src/theme.cpp ../src/drawing.cpp ../src/canvas.cpp ../src/canvasimages.cpp
HEADERS += ../src/engine.h ../src/theme.h ../src/canvas.h
